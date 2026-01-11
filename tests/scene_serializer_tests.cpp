#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

TEST(SceneSerializer, LightBakeFieldsRoundtrip)
{
  std::filesystem::create_directories("assets/scenes/test");
  const std::string filepath = "assets/scenes/test/test_scene_bake_roundtrip.json";

  // Setup minimal device/context required by ResourceManager
  Window          win(16, 16, "Test");
  Device          device(win);
  ResourceManager rm(device);

  // Create a scene with one directional light and bake fields set
  Scene scene;
  auto  entity = scene.createEntity();
  auto& dl     = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  dl.intensity = 1.23f;
  dl.color     = glm::vec3(0.5f, 0.6f, 0.7f);
  dl.bake      = true;
  dl.lightType = LightMobility::Dynamic;

  SceneSerializer serializer(scene, rm);
  serializer.serialize(filepath);

  // Deserialize into a fresh scene
  Scene           scene2;
  SceneSerializer serializer2(scene2, rm);
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto view = scene2.getRegistry().view<DirectionalLightComponent>();
  ASSERT_EQ(std::distance(view.begin(), view.end()), 1);

  for (auto e : view)
  {
    auto& d2 = scene2.getRegistry().get<DirectionalLightComponent>(e);
    EXPECT_TRUE(d2.bake);
    EXPECT_EQ(d2.lightType, LightMobility::Dynamic);
    EXPECT_FLOAT_EQ(d2.intensity, 1.23f);
    EXPECT_FLOAT_EQ(d2.color.x, 0.5f);
  }
}

TEST(SceneSerializer, DeserializeDemoBakeScene)
{
  std::string const path = "assets/scenes/test/demo_scene_bake.json";

  if (!std::filesystem::exists(path)) GTEST_SKIP() << "Missing demo scene file: " << path;

  Window          win(16, 16, "Test");
  Device          device(win);
  ResourceManager rm(device);

  Scene           scene;
  SceneSerializer serializer(scene, rm);
  ASSERT_TRUE(serializer.deserialize(path));

  // Verify directional and point light components exist and have bake flags
  auto dirView = scene.getRegistry().view<DirectionalLightComponent>();
  ASSERT_GT(std::distance(dirView.begin(), dirView.end()), 0);
  for (auto e : dirView)
  {
    auto& dl = scene.getRegistry().get<DirectionalLightComponent>(e);
    EXPECT_TRUE(dl.bake);
    EXPECT_EQ(dl.lightType, LightMobility::Static);
  }

  auto pointView = scene.getRegistry().view<PointLightComponent>();
  ASSERT_GT(std::distance(pointView.begin(), pointView.end()), 0);
  for (auto e : pointView)
  {
    auto& pl = scene.getRegistry().get<PointLightComponent>(e);
    EXPECT_FALSE(pl.bake);
    EXPECT_EQ(pl.lightType, LightMobility::Dynamic);
  }
}
