#include <gtest/gtest.h>

#include "ModelLib/Resources/PBRMaterial.hpp"
#include "Engine/Scene/LightmapManifest.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

TEST(SceneBindings, ApplyBindingsToScene)
{
  // Create a small manifest in memory
  std::unordered_map<std::string, engine::scene::LightmapInfo>    infos;
  std::unordered_map<std::string, engine::scene::LightmapBinding> binds;

  engine::scene::LightmapInfo li;
  li.id        = "lm_000";
  li.file      = "lightmaps/lm_000_atlas.vtex";
  infos[li.id] = li;

  engine::scene::LightmapBinding b;
  b.lightmapId       = "lm_000";
  b.uvChannel        = 1;
  b.uvScale          = {0.25f, 0.25f};
  b.uvOffset         = {0.5f, 0.0f};
  binds["object_01"] = b;

  // Create scene and entity
  engine::Scene scene;
  auto          entity = scene.createEntity();
  scene.getRegistry().emplace<engine::NameComponent>(entity, "object_01");
  scene.getRegistry().emplace<engine::PBRMaterial>(entity);

  // Apply bindings
  engine::scene::applyBindingsToScene(binds, scene);

  // Verify LightmapComponent exists and fields match
  ASSERT_TRUE(scene.getRegistry().all_of<engine::LightmapComponent>(entity));
  auto& lm = scene.getRegistry().get<engine::LightmapComponent>(entity);
  EXPECT_EQ(lm.lightmapId, "lm_000");
  EXPECT_FLOAT_EQ(lm.uvScale.x, 0.25f);
  EXPECT_FLOAT_EQ(lm.uvOffset.x, 0.5f);

  // Verify PBRMaterial uvScale updated
  auto& mat = scene.getRegistry().get<engine::PBRMaterial>(entity);
  EXPECT_FLOAT_EQ(mat.uvScale, 0.25f);
}

