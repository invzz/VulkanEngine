#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

TEST(LightmapRenderSmoke, BuildSceneLoadLightmapAndRunRendererFrame)
{
  // Prepare demo scene + manifest directories
  std::filesystem::create_directories("assets/scenes");
  std::filesystem::create_directories("assets/lightmaps");

  const std::string scenePath    = "assets/scenes/demo_scene.json";
  const std::string manifestPath = "assets/scenes/demo_scene_lightmaps.json";
  const std::string lightmapPath = "assets/lightmaps/lm_000_atlas.vtex";

  // Small demo scene: a single named object with default material
  {
    std::ofstream out(scenePath);
    out << R"({ "objects": [ { "id": "object_01", "name": "object_01" } ] })";
    out.close();
  }

  // Manifest referencing a lightmap atlas (we'll write a small VTEX below)
  {
    std::ofstream out(manifestPath);
    out << R"({
      "version": 1,
      "lightmapBindings": {
        "object_01": { "lightmapId": "lm_000", "uvChannel": 1, "uvScale": [1.0, 1.0], "uvOffset": [0.0, 0.0] }
      },
      "lightmaps": [ { "id": "lm_000", "file": "lightmaps/lm_000_atlas.vtex", "format": "vtex", "resolution": [4, 4], "usage": "Lightmap" } ]
    })";
    out.close();
  }

  // Create a small 4x4 RGBA R32F image and write a .vtex using the helper (same pattern as existing tests)
  {
    const uint32_t     w = 4, h = 4;
    std::vector<float> pixels(static_cast<size_t>(w) * h * 4, 0.0f);
    for (uint32_t y = 0; y < h; ++y)
    {
      for (uint32_t x = 0; x < w; ++x)
      {
        size_t idx      = (y * w + x) * 4;
        pixels[idx + 0] = 0.25f; // R
        pixels[idx + 1] = 0.5f;  // G
        pixels[idx + 2] = 0.75f; // B
        pixels[idx + 3] = 1.0f;  // A
      }
    }

    bool ok = ibl_detail::vtex::writeImageFromRaw(lightmapPath, reinterpret_cast<const unsigned char*>(pixels.data()), pixels.size() * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, w, h, 1, 1);
    ASSERT_TRUE(ok) << "Failed to write demo VTEX to " << lightmapPath;
  }

  // Setup Window/Device/Renderer and ResourceManager
  Window          win(64, 64, "RenderSmoke");
  Device          device(win);
  Renderer        renderer(win, device);
  ResourceManager rm(device);

  // Load scene + manifest, then load textures and apply
  Scene           scene;
  SceneSerializer serializer(scene, rm);
  ASSERT_TRUE(serializer.deserialize(scenePath));

  ASSERT_TRUE(rm.loadSceneLightmapManifest(manifestPath));
  rm.applySceneLightmapBindings(scene);

  ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));
  rm.applyLoadedLightmapsToScene(scene);

  // Basic sanity: textures registered and material has lightmap set where applicable
  auto lmView = scene.getRegistry().view<engine::LightmapComponent>();
  ASSERT_GT(std::distance(lmView.begin(), lmView.end()), 0);
  for (auto e : lmView)
  {
    auto& lm     = scene.getRegistry().get<engine::LightmapComponent>(e);
    auto  texOpt = rm.getLightmapInfoById(lm.lightmapId);
    ASSERT_TRUE(texOpt.has_value());
  }

  // Run one renderer frame (smoke test — ensures renderer/command submission + offscreen path don't crash)
  VkCommandBuffer cb = renderer.beginFrame();
  // No scene graph submission here — just exercise frame lifecycle
  renderer.endFrame();

  SUCCEED();
}
