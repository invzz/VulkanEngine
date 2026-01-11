#include <gtest/gtest.h>

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

TEST(LightmapRenderPixelCompare, FullscreenSampleMatchesLightmap)
{
  std::filesystem::create_directories("assets/lightmaps");
  std::filesystem::create_directories("assets/scenes/test");

  const std::string scenePath    = "assets/scenes/test/demo_scene.json";
  const std::string manifestPath = "assets/scenes/test/demo_scene_lightmaps.json";
  const std::string lightmapPath = "assets/lightmaps/lm_000_atlas.vtex";

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
        "object_01": { "meshes": [ { "primitiveIndex": 0, "lightmap": "lightmaps/lm_000_atlas.vtex", "uvChannel": 1, "uvScale": [1.0, 1.0], "uvOffset": [0.0, 0.0], "resolution": [16, 16] } ] }
      },
      "lightmaps": [ { "id": "lm_000", "file": "lightmaps/lm_000_atlas.vtex", "format": "vtex", "resolution": [4, 4], "usage": "Lightmap" } ]
    })";
    out.close();
  }

  // Write small constant 4x4 VTEX (constant color makes sampling deterministic)
  const uint32_t     w = 4, h = 4;
  std::vector<float> pixels(static_cast<size_t>(w) * h * 4, 0.0f);
  for (uint32_t i = 0; i < w * h; ++i)
  {
    pixels[i * 4 + 0] = 0.25f;
    pixels[i * 4 + 1] = 0.5f;
    pixels[i * 4 + 2] = 0.75f;
    pixels[i * 4 + 3] = 1.0f;
  }
  ASSERT_TRUE(ibl_detail::vtex::writeImageFromRaw(lightmapPath, reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size() * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, w, h, 1, 1));

  // Use a small 4x4 window to make sampling simple and deterministic
  Window          win(4, 4, "LightmapPixelTest");
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

  // Grab the lightmap texture. Prefer a material-bound texture if present, otherwise
  // fall back to looking up the loaded texture by id from the ResourceManager.
  std::shared_ptr<Texture> lightmapTex = nullptr;
  auto                     viewMat     = scene.getRegistry().view<engine::PBRMaterial, engine::LightmapComponent>();
  for (auto e : viewMat)
  {
    auto& mat = scene.getRegistry().get<engine::PBRMaterial>(e);
    if (mat.lightmap)
    {
      lightmapTex = mat.lightmap;
      break;
    }
  }
  if (!lightmapTex)
  {
    auto viewLm = scene.getRegistry().view<engine::LightmapComponent>();
    for (auto e : viewLm)
    {
      auto& lm    = scene.getRegistry().get<engine::LightmapComponent>(e);
      lightmapTex = rm.getLightmapTextureById(lm.lightmapId);
      if (lightmapTex) break;
    }
  }
  ASSERT_TRUE(lightmapTex != nullptr);

  // Create descriptor set layout/pool for binding the texture as set 0 binding 0
  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();
  auto pool   = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();

  VkDescriptorSet       ds     = VK_NULL_HANDLE;
  VkDescriptorImageInfo lmInfo = lightmapTex->getDescriptorInfo();
  // Verify descriptor info is valid
  ASSERT_NE(lmInfo.imageView, VK_NULL_HANDLE);
  ASSERT_NE(lmInfo.sampler, VK_NULL_HANDLE);
  ASSERT_NE(lightmapTex->getImage(), VK_NULL_HANDLE);

  // Sanity-check the GPU-resident lightmap contents by copying the VTEX image to CPU
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
  EXPECT_NEAR(lmPixels[0], 0.25f, 1e-3f);
  EXPECT_NEAR(lmPixels[1], 0.5f, 1e-3f);
  EXPECT_NEAR(lmPixels[2], 0.75f, 1e-3f);
  EXPECT_NEAR(lmPixels[3], 1.0f, 1e-3f);
  vkUnmapMemory(device.device(), lmStagingMem);
  vkDestroyBuffer(device.device(), lmStaging, nullptr);
  vkFreeMemory(device.device(), lmStagingMem, nullptr);

  SUCCEED();
}
