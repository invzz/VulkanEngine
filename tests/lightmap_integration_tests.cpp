#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/LightmapManifest.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

TEST(LightmapIntegration, ExportLoadApplyAndAssignTexture)
{
  // Create a small authoring scene and manifest on disk
  std::filesystem::create_directories("assets/scenes/test");
  const std::string scenePath    = "assets/scenes/test/demo_scene.json";
  const std::string manifestPath = "assets/scenes/test/demo_scene_lightmaps.json";

  {
    std::ofstream out(scenePath);
    out << R"({ "objects": [ { "id": "object_01", "name": "object_01", "material": { "albedo": [1.0, 1.0, 1.0], "metallic": 0.0, "roughness": 0.5, "ao": 1.0 } } ] })";
    out.close();
  }

  {
    std::ofstream out(manifestPath);
    out << R"({
      "version": 1,
      "lightmapBindings": {
        "object_01": { "lightmapId": "lm_000", "uvChannel": 1, "uvScale": [0.25, 0.25], "uvOffset": [0.5, 0.0] }
      },
      "lightmaps": [ { "id": "lm_000", "file": "lightmaps/lm_000_atlas.vtex", "format": "vtex", "resolution": [32, 32], "usage": "Lightmap" } ]
    })";
    out.close();
  }

  // Setup real Window/Device (required for Texture creation)
  Window          window(1, 1, "LightmapTestWindow");
  Device          device(window);
  ResourceManager rm(device);

  // Deserialize scene
  Scene           scene;
  SceneSerializer serializer(scene, rm);
  ASSERT_TRUE(serializer.deserialize(scenePath));

  // Load manifest into ResourceManager and apply bindings
  ASSERT_TRUE(rm.loadSceneLightmapManifest(manifestPath));
  rm.applySceneLightmapBindings(scene);

  // Sanity checks: scene should have a NameComponent and a PBRMaterial
  auto   nameView  = scene.getRegistry().view<engine::NameComponent>();
  size_t nameCount = 0;
  for (auto e : nameView)
    ++nameCount;
  ASSERT_EQ(nameCount, 1u);

  // Scene doesn't automatically create a PBRMaterial unless a model is present; create one for the test entity
  for (auto e : nameView)
  {
    const auto& nc = scene.getRegistry().get<engine::NameComponent>(e);
    if (nc.name == "object_01")
    {
      if (!scene.getRegistry().all_of<engine::PBRMaterial>(e))
      {
        scene.getRegistry().emplace<engine::PBRMaterial>(e);
      }
    }
  }

  auto   matView  = scene.getRegistry().view<engine::PBRMaterial>();
  size_t matCount = 0;
  for (auto e : matView)
    ++matCount;
  ASSERT_EQ(matCount, 1u);

  // Apply bindings should emplace LightmapComponent
  auto   lmViewBefore  = scene.getRegistry().view<engine::LightmapComponent>();
  size_t lmCountBefore = 0;
  for (auto e : lmViewBefore)
    ++lmCountBefore;
  ASSERT_EQ(lmCountBefore, 1u);

  for (auto entity : lmViewBefore)
  {
    auto& lm = scene.getRegistry().get<engine::LightmapComponent>(entity);
    EXPECT_EQ(lm.lightmapId, "lm_000");

    // Auto-load textures referenced by the manifest (use assets as base path)
    ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));

    // Apply loaded textures to the scene
    rm.applyLoadedLightmapsToScene(scene);

    auto& mat = scene.getRegistry().get<engine::PBRMaterial>(entity);
    EXPECT_TRUE(mat.lightmap != nullptr);
    EXPECT_TRUE(mat.hasLightmap());
  }
}
