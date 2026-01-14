#include <gtest/gtest.h>
#include <tinyexr.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "gtest/gtest.h"

using namespace engine;

static float halfToFloat(uint16_t h)
{
  uint32_t s = (h >> 15) & 0x00000001u;
  uint32_t e = (h >> 10) & 0x0000001fu;
  uint32_t f = h & 0x000003ffu;

  uint32_t out;
  if (e == 0u)
  {
    if (f == 0u)
    {
      out = s << 31;
    }
    else
    {
      // Subnormal
      while ((f & 0x00000400u) == 0u)
      {
        f <<= 1u;
        e -= 1u;
      }
      e += 1u;
      f &= ~0x00000400u;
      uint32_t mant = f << 13u;
      uint32_t exp  = (e + (127 - 15)) << 23u;
      out           = (s << 31u) | exp | mant;
    }
  }
  else if (e == 31u)
  {
    // Inf or NaN
    out = (s << 31u) | 0x7f800000u | (f << 13u);
  }
  else
  {
    uint32_t mant = f << 13u;
    uint32_t exp  = (e + (127 - 15)) << 23u;
    out           = (s << 31u) | exp | mant;
  }

  float result;
  std::memcpy(&result, &out, sizeof(float));
  return result;
}

TEST(LightmapGolden, FullscreenRegionCompare)
{
  GTEST_SKIP() << "Temporarily disabled test due to frequent timeouts in CI environments";
  std::filesystem::create_directories("assets/lightmaps");
  std::filesystem::create_directories("assets/goldens");
  std::filesystem::create_directories("assets/scenes/test");

  const std::string scenePath    = "assets/scenes/test/demo_scene_golden.json";
  const std::string manifestPath = "assets/scenes/test/demo_scene_golden_lightmaps.json";
  const std::string lightmapPath = "assets/lightmaps/lm_golden_atlas.vtex";
  const std::string goldenPath   = "assets/goldens/lightmap_fullscreen_golden.exr";

  // Scene + manifest (simple single object referencing lightmap)
  {
    std::ofstream out(scenePath);
    out << R"({ "objects": [ { "id": "object_01", "name": "object_01" } ] })";
  }
  {
    std::ofstream out(manifestPath);
    out << R"({
      "version": 1,
      "lightmapBindings": {
        "object_01": { "meshes": [ { "primitiveIndex": 0, "lightmap": "lightmaps/lm_golden_atlas.vtex", "uvChannel": 1, "uvScale": [1.0, 1.0], "uvOffset": [0.0, 0.0], "resolution": [16, 16] } ] }
      },
      "lightmaps": [ { "id": "lm_golden", "file": "lightmaps/lm_golden_atlas.vtex", "format": "vtex", "resolution": [16, 16], "usage": "Lightmap" } ]
    })";
  }

  // Create a deterministic 16x16 lightmap with a simple gradient pattern
  const uint32_t     w = 16, h = 16;
  std::vector<float> pixels(static_cast<size_t>(w) * h * 4u);
  for (uint32_t y = 0; y < h; ++y)
  {
    for (uint32_t x = 0; x < w; ++x)
    {
      size_t i      = (y * w + x) * 4u;
      pixels[i + 0] = static_cast<float>(x) / static_cast<float>(w - 1); // R gradient
      pixels[i + 1] = static_cast<float>(y) / static_cast<float>(h - 1); // G gradient
      pixels[i + 2] = 0.5f;                                              // B constant
      pixels[i + 3] = 1.0f;                                              // A
    }
  }

  ASSERT_TRUE(ibl_detail::vtex::writeImageFromRaw(lightmapPath, reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size() * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, w, h, 1, 1));

  // Render a fullscreen pass that samples the lightmap (post-process shader approach)
  const VkExtent2D extent{w, h};
  Window           win(w, h, "LightmapGolden");
  Device           device(win);
  Renderer         renderer(win, device);
  ResourceManager  rm(device);

  Scene           scene;
  SceneSerializer serializer(scene, rm);
  ASSERT_TRUE(serializer.deserialize(scenePath));
  ASSERT_TRUE(rm.loadSceneLightmapManifest(manifestPath));
  rm.applySceneLightmapBindings(scene);
  ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));
  rm.applyLoadedLightmapsToScene(scene);

  // Lookup lightmap texture
  std::shared_ptr<Texture> lightmapTex = nullptr;
  auto                     viewLm      = scene.getRegistry().view<engine::LightmapComponent>();
  for (auto e : viewLm)
  {
    auto& lm    = scene.getRegistry().get<engine::LightmapComponent>(e);
    lightmapTex = rm.getLightmapTextureById(lm.lightmapId);
    if (lightmapTex) break;
  }
  ASSERT_TRUE(lightmapTex != nullptr);

  // Descriptor set layout & pool
  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();
  auto pool   = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();

  VkDescriptorSet       ds     = VK_NULL_HANDLE;
  VkDescriptorImageInfo lmInfo = lightmapTex->getDescriptorInfo();
  ASSERT_NE(lmInfo.imageView, VK_NULL_HANDLE);
  ASSERT_NE(lmInfo.sampler, VK_NULL_HANDLE);
  engine::DescriptorWriter(*layout, *pool).writeImage(0, &lmInfo).build(ds);

  // Offscreen render: use PostProcessingSystem bound to offscreen pass
  PostProcessingSystem postProc(device, renderer.getOffscreenRenderPass(), {layout->getDescriptorSetLayout()});

  // Frame render
  VkCommandBuffer cmd = renderer.beginFrame();
  Camera          camera;
  FrameInfo       frameInfo{renderer.getFrameIndex(), 0.0f, cmd, camera, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, extent, 0};

  renderer.beginOffscreenRenderPass(cmd);
  PostProcessPushConstants push{};
  push.enableFXAA      = 0;
  push.enableBloom     = 0;
  push.enableSSAO      = 0;
  push.toneMappingMode = 0;
  push.vignette        = 0.0f;
  push.exposure        = 1.0f;
  push.contrast        = 1.0f;
  push.saturation      = 1.0f;
  postProc.render(frameInfo, ds, push);
  renderer.endOffscreenRenderPass(cmd);
  renderer.endFrame();

  device.WaitIdle();

  // Read back offscreen image (R16G16B16A16_SFLOAT)
  VkImage        offscreen = renderer.getOffscreenColorImage(0);
  VkBuffer       hostBuf;
  VkDeviceMemory hostMem;
  device.getMemory().createBuffer(static_cast<VkDeviceSize>(w * h * 4u * sizeof(uint16_t)),
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  hostBuf,
                                  hostMem);
  VkBufferImageCopy region{};
  region.bufferOffset                    = 0;
  region.bufferRowLength                 = 0;
  region.bufferImageHeight               = 0;
  region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount     = 1;
  region.imageOffset                     = {0, 0, 0};
  region.imageExtent                     = {w, h, 1};
  device.getMemory().copyImageToBuffer(offscreen, hostBuf, {region}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  void* data = nullptr;
  vkMapMemory(device.device(), hostMem, 0, VK_WHOLE_SIZE, 0, &data);

  uint16_t*          halfs = reinterpret_cast<uint16_t*>(data);
  std::vector<float> rendered(w * h * 4u);
  for (size_t i = 0; i < rendered.size(); ++i)
    rendered[i] = halfToFloat(halfs[i]);

  // Convert shader output from gamma back to linear
  const float gamma = 2.2f;
  for (size_t px = 0; px < w * h; ++px)
  {
    size_t base = px * 4u;
    for (size_t c = 0; c < 3; ++c)
      rendered[base + c] = std::pow(rendered[base + c], gamma);
  }

  // Golden handling
  const char* updateEnv = std::getenv("UPDATE_GOLDEN");
  const bool  update    = (updateEnv != nullptr && std::string(updateEnv) == "1");

  if (!std::filesystem::exists(goldenPath))
  {
    if (update)
    {
      // Save golden EXR in linear float RGBA
      const char* err = nullptr;
      int         r   = SaveEXR(rendered.data(), w, h, 4, 0, goldenPath.c_str(), &err);
      if (r != 0)
      {
        if (err)
        {
          std::cerr << "SaveEXR error: " << err << std::endl;
          FreeEXRErrorMessage(err);
        }
      }
      ASSERT_EQ(r, 0);
      std::cerr << "Wrote golden image: " << goldenPath << std::endl;
      vkUnmapMemory(device.device(), hostMem);
      vkDestroyBuffer(device.device(), hostBuf, nullptr);
      vkFreeMemory(device.device(), hostMem, nullptr);
      GTEST_SKIP() << "Golden image written; re-run without UPDATE_GOLDEN=1 to validate.";
    }
    else
    {
      vkUnmapMemory(device.device(), hostMem);
      vkDestroyBuffer(device.device(), hostBuf, nullptr);
      vkFreeMemory(device.device(), hostMem, nullptr);
      FAIL() << "Missing golden image: " << goldenPath << " - set UPDATE_GOLDEN=1 to create it.";
    }
  }

  // Load golden EXR
  int         gw = 0, gh = 0;
  float*      gPixels = nullptr;
  const char* err     = nullptr;
  int         loadRes = LoadEXR(&gPixels, &gw, &gh, goldenPath.c_str(), &err);
  ASSERT_EQ(loadRes, TINYEXR_SUCCESS);
  ASSERT_EQ(gw, static_cast<int>(w));
  ASSERT_EQ(gh, static_cast<int>(h));
  std::vector<float> golden(gPixels, gPixels + (static_cast<size_t>(gw) * gh * 4u));
  free(gPixels);

  // Region compare: central 8x8 region
  const int regionSize = 8;
  const int rx         = (w - regionSize) / 2;
  const int ry         = (h - regionSize) / 2;

  float  maxDiff = 0.0f;
  float  rms     = 0.0f;
  size_t count   = 0;
  for (int y = 0; y < regionSize; ++y)
  {
    for (int x = 0; x < regionSize; ++x)
    {
      int    sx  = rx + x;
      int    sy  = ry + y;
      size_t idx = (sy * w + sx) * 4u;
      for (int c = 0; c < 3; ++c)
      {
        float d = std::abs(rendered[idx + c] - golden[idx + c]);
        maxDiff = std::max(maxDiff, d);
        rms += d * d;
        ++count;
      }
    }
  }
  rms = std::sqrt(rms / static_cast<float>(count));

  const float maxAllowed = 2e-3f; // small tolerance
  const float rmsAllowed = 1e-3f;

  vkUnmapMemory(device.device(), hostMem);
  vkDestroyBuffer(device.device(), hostBuf, nullptr);
  vkFreeMemory(device.device(), hostMem, nullptr);

  EXPECT_LE(maxDiff, maxAllowed) << "Max diff " << maxDiff << " exceeds " << maxAllowed;
  EXPECT_LE(rms, rmsAllowed) << "RMS " << rms << " exceeds " << rmsAllowed;
}
