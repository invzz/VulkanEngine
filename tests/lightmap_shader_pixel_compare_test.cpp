#include <gtest/gtest.h>

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

using namespace engine;

TEST(LightmapShaderCompare, PostProcessSamplerMatchesLightmap)
{
  std::filesystem::create_directories("assets/lightmaps");
  std::filesystem::create_directories("assets/scenes");

  const std::string scenePath    = "assets/scenes/test/demo_scene_shader.json";
  const std::string manifestPath = "assets/scenes/test/demo_scene_shader_lightmaps.json";
  const std::string lightmapPath = "assets/lightmaps/lm_001_atlas.vtex";

  // Scene + manifest
  {
    std::ofstream out(scenePath);
    out << R"({ "objects": [ { "id": "object_01", "name": "object_01" } ] })";
    out.close();
  }
  {
    std::ofstream out(manifestPath);
    out << R"({
      "version": 1,
      "lightmapBindings": {
        "object_01": { "meshes": [ { "primitiveIndex": 0, "lightmap": "lightmaps/lm_001_atlas.vtex", "uvChannel": 1, "uvScale": [1.0, 1.0], "uvOffset": [0.0, 0.0], "resolution": [16, 16] } ] }
      },

      "lightmaps": [ { "id": "lm_001", "file": "lightmaps/lm_001_atlas.vtex", "format": "vtex", "resolution": [4, 4], "usage": "Lightmap" } ]
    })";
    out.close();
  }

  // Write constant 4x4 VTEX (deterministic color)
  const uint32_t     w = 4, h = 4;
  std::vector<float> pixels(static_cast<size_t>(w) * h * 4, 0.0f);
  for (uint32_t i = 0; i < w * h; ++i)
  {
    pixels[i * 4 + 0] = 0.125f;
    pixels[i * 4 + 1] = 0.25f;
    pixels[i * 4 + 2] = 0.375f;
    pixels[i * 4 + 3] = 1.0f;
  }
  ASSERT_TRUE(ibl_detail::vtex::writeImageFromRaw(lightmapPath, reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size() * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, w, h, 1, 1));

  // Small 4x4 offscreen to render and read back
  Window          win(4, 4, "LightmapShaderCompare");
  Device          device(win);
  Renderer        renderer(win, device);
  ResourceManager rm(device);

  // Load scene + manifest + textures
  Scene           scene;
  SceneSerializer serializer(scene, rm);
  ASSERT_TRUE(serializer.deserialize(scenePath));
  ASSERT_TRUE(rm.loadSceneLightmapManifest(manifestPath));
  rm.applySceneLightmapBindings(scene);
  ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));
  rm.applyLoadedLightmapsToScene(scene);

  // Lookup the loaded lightmap texture
  std::shared_ptr<Texture> lightmapTex = nullptr;
  auto                     viewLm      = scene.getRegistry().view<engine::LightmapComponent>();
  for (auto e : viewLm)
  {
    auto& lm    = scene.getRegistry().get<engine::LightmapComponent>(e);
    lightmapTex = rm.getLightmapTextureById(lm.lightmapId);
    if (lightmapTex) break;
  }
  ASSERT_TRUE(lightmapTex != nullptr);

  // Create descriptor set layout with bindings (sceneColor, depthMap) to match post_process.frag
  auto layout = DescriptorSetLayout::Builder(device)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .build();
  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2).build();

  VkDescriptorSet       ds     = VK_NULL_HANDLE;
  VkDescriptorImageInfo lmInfo = lightmapTex->getDescriptorInfo();
  ASSERT_NE(lmInfo.imageView, VK_NULL_HANDLE);
  ASSERT_NE(lmInfo.sampler, VK_NULL_HANDLE);

  // Build descriptor set with only binding 0 written
  engine::DescriptorWriter(*layout, *pool).writeImage(0, &lmInfo).build(ds);
  ASSERT_NE(ds, VK_NULL_HANDLE);

  // Sanity-check GPU-resident lightmap contents before sampling
  VkImage        lmImage = lightmapTex->getImage();
  VkBuffer       lmStaging;
  VkDeviceMemory lmStagingMem;
  device.getMemory().createBuffer(static_cast<VkDeviceSize>(lightmapTex->getWidth() * lightmapTex->getHeight() * 4 * sizeof(float)),
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  lmStaging,
                                  lmStagingMem);
  VkBufferImageCopy lmRegion{};
  lmRegion.bufferOffset                    = 0;
  lmRegion.bufferRowLength                 = 0;
  lmRegion.bufferImageHeight               = 0;
  lmRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  lmRegion.imageSubresource.mipLevel       = 0;
  lmRegion.imageSubresource.baseArrayLayer = 0;
  lmRegion.imageSubresource.layerCount     = 1;
  lmRegion.imageOffset                     = {0, 0, 0};
  lmRegion.imageExtent                     = {static_cast<uint32_t>(lightmapTex->getWidth()), static_cast<uint32_t>(lightmapTex->getHeight()), 1};
  device.getMemory().copyImageToBuffer(lmImage, lmStaging, {lmRegion}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  void* lmData = nullptr;
  vkMapMemory(device.device(), lmStagingMem, 0, VK_WHOLE_SIZE, 0, &lmData);
  float* lmPixels = reinterpret_cast<float*>(lmData);
  EXPECT_NEAR(lmPixels[0], 0.125f, 1e-3f);
  EXPECT_NEAR(lmPixels[1], 0.25f, 1e-3f);
  EXPECT_NEAR(lmPixels[2], 0.375f, 1e-3f);
  EXPECT_NEAR(lmPixels[3], 1.0f, 1e-3f);

  // Keep a ground-truth copy of the lightmap pixels (linear floats)
  std::vector<float> groundPixels(4 * 4 * 4);
  for (size_t i = 0; i < groundPixels.size(); ++i)
    groundPixels[i] = lmPixels[i];

  vkUnmapMemory(device.device(), lmStagingMem);
  vkDestroyBuffer(device.device(), lmStaging, nullptr);
  vkFreeMemory(device.device(), lmStagingMem, nullptr);

  // Create post-process system bound to the offscreen render pass so we can read back its color image
  PostProcessingSystem postProc(device, renderer.getOffscreenRenderPass(), {layout->getDescriptorSetLayout()});

  // Begin frame and render a fullscreen pass that samples our lightmap via binding 0
  VkCommandBuffer cmd        = renderer.beginFrame();
  int             frameIndex = renderer.getFrameIndex();

  // Simple camera & frame info (initialize Camera reference in ctor)
  Camera    camera;
  FrameInfo frameInfo{frameIndex, 0.0f, cmd, camera, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {4, 4}, 0};

  renderer.beginOffscreenRenderPass(cmd);

  PostProcessPushConstants push{};
  push.enableFXAA      = 0;
  push.enableBloom     = 0;
  push.enableSSAO      = 0;
  push.toneMappingMode = 0;    // disable tone mapping
  push.vignette        = 0.0f; // disable vignette for deterministic sampling
  push.exposure        = 1.0f;
  push.contrast        = 1.0f;
  push.saturation      = 1.0f;
  postProc.render(frameInfo, ds, push);

  renderer.endOffscreenRenderPass(cmd);
  renderer.endFrame();

  device.WaitIdle();

  // Read back offscreen color image
  VkImage        offscreen = renderer.getOffscreenColorImage(0);
  VkBuffer       hostBuf;
  VkDeviceMemory hostMem;
  device.getMemory().createBuffer(static_cast<VkDeviceSize>(4u * 4u * 4u * sizeof(float)),
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
  region.imageExtent                     = {4, 4, 1};

  device.getMemory().copyImageToBuffer(offscreen, hostBuf, {region}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  void* data = nullptr;
  vkMapMemory(device.device(), hostMem, 0, VK_WHOLE_SIZE, 0, &data);

  // Offscreen uses R16G16B16A16_SFLOAT; convert half-floats to float for comparison
  auto halfToFloat = [](uint16_t h) {
    uint32_t s = (h >> 15) & 0x00000001;
    uint32_t e = (h >> 10) & 0x0000001f;
    uint32_t f = h & 0x000003ff;

    uint32_t out;
    if (e == 0)
    {
      if (f == 0)
      {
        out = s << 31;
      }
      else
      {
        // Subnormal
        while ((f & 0x00000400) == 0)
        {
          f <<= 1;
          e -= 1;
        }
        e += 1;
        f &= ~0x00000400;
        uint32_t mant = f << 13;
        uint32_t exp  = (e + (127 - 15)) << 23;
        out           = (s << 31) | exp | mant;
      }
    }
    else if (e == 31)
    {
      // Inf or NaN
      out = (s << 31) | 0x7f800000 | (f << 13);
    }
    else
    {
      uint32_t mant = f << 13;
      uint32_t exp  = (e + (127 - 15)) << 23;
      out           = (s << 31) | exp | mant;
    }

    float result;
    std::memcpy(&result, &out, sizeof(float));
    return result;
  };

  uint16_t*          halfs = reinterpret_cast<uint16_t*>(data);
  std::vector<float> shaderPixels(4 * 4 * 4);
  for (size_t i = 0; i < shaderPixels.size(); ++i)
    shaderPixels[i] = halfToFloat(halfs[i]);

  // Convert shader output from gamma (display) space back to linear for comparison
  const float gamma = 2.2f;
  for (size_t px = 0; px < 4 * 4; ++px)
  {
    size_t base = px * 4;
    for (size_t c = 0; c < 3; ++c)
    {
      shaderPixels[base + c] = std::pow(shaderPixels[base + c], gamma);
    }
    // alpha left unchanged
  }

  // Compare shader-rendered pixels (converted back to linear) to the ground-truth lightmap values we captured earlier
  for (size_t i = 0; i < 4u * 4u * 4u; ++i)
  {
    EXPECT_NEAR(shaderPixels[i], groundPixels[i], 1e-3f) << "Mismatch at index " << i << " (shader vs ground-truth lightmap)";
  }

  // If any pixel mismatches, emit a concise debug failure message with a small sample
  bool mismatch = false;
  for (size_t i = 0; i < shaderPixels.size(); ++i)
  {
    if (std::abs(shaderPixels[i] - groundPixels[i]) > 1e-3f)
    {
      mismatch = true;
      break;
    }
  }
  if (mismatch)
  {
    ADD_FAILURE() << "Shader output differs from ground-truth lightmap. Shader[0..3] = (" << shaderPixels[0] << ", " << shaderPixels[1] << ", " << shaderPixels[2] << ", " << shaderPixels[3]
                  << ") vs Ground[0..3] = (" << groundPixels[0] << ", " << groundPixels[1] << ", " << groundPixels[2] << ", " << groundPixels[3] << ")";
  }

  // Unmap and free staging buffer used for shader readback
  vkUnmapMemory(device.device(), hostMem);
  vkDestroyBuffer(device.device(), hostBuf, nullptr);
  vkFreeMemory(device.device(), hostMem, nullptr);

  SUCCEED();
}
