#include <gtest/gtest.h>

#include <filesystem>

#include "../fixtures/SceneFixture.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

// Use SceneFixture which provides Device + ResourceManager
class SceneSerializerTest : public engine::test::SceneFixture {};

// =============================================================================
// Light Bake Fields Roundtrip Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenDirectionalLightWithBakeFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
  const std::string filepath = "assets/scenes/test/test_scene_bake_roundtrip.json";

  // Create a scene with one directional light and bake fields set
  Scene scene;
  auto entity = scene.createEntity();
  auto& dl = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  dl.intensity = 1.23f;
  dl.color = glm::vec3(0.5f, 0.6f, 0.7f);
  dl.bake = true;
  dl.lightType = LightMobility::Dynamic;

  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  // Deserialize into a fresh scene
  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto view = scene2.getRegistry().view<DirectionalLightComponent>();
  ASSERT_EQ(std::distance(view.begin(), view.end()), 1);

  for (auto e : view) {
    auto& d2 = scene2.getRegistry().get<DirectionalLightComponent>(e);
    EXPECT_TRUE(d2.bake);
    EXPECT_EQ(d2.lightType, LightMobility::Dynamic);
    EXPECT_FLOAT_EQ(d2.intensity, 1.23f);
    EXPECT_FLOAT_EQ(d2.color.x, 0.5f);
  }
}

// =============================================================================
// Demo Scene Loading Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenDemoBakeSceneFile_WhenDeserialized_ThenLightComponentsHaveCorrectBakeFlags) {
  std::string const path = "assets/scenes/test/demo_scene_bake.json";

  if (!std::filesystem::exists(path)) {
    GTEST_SKIP() << "Missing demo scene file: " << path;
  }

  Scene scene;
  SceneSerializer serializer(scene, resourceManager());
  ASSERT_TRUE(serializer.deserialize(path));

  // Verify directional light components exist and have bake flags
  auto dirView = scene.getRegistry().view<DirectionalLightComponent>();
  ASSERT_GT(std::distance(dirView.begin(), dirView.end()), 0);
  for (auto e : dirView) {
    auto& dl = scene.getRegistry().get<DirectionalLightComponent>(e);
    EXPECT_TRUE(dl.bake);
    EXPECT_EQ(dl.lightType, LightMobility::Static);
  }

  // Verify point light components exist and have bake flags
  auto pointView = scene.getRegistry().view<PointLightComponent>();
  ASSERT_GT(std::distance(pointView.begin(), pointView.end()), 0);
  for (auto e : pointView) {
    auto& pl = scene.getRegistry().get<PointLightComponent>(e);
    EXPECT_FALSE(pl.bake);
    EXPECT_EQ(pl.lightType, LightMobility::Dynamic);
  }
}

// =============================================================================
// PointLight Roundtrip Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenPointLightWithAllFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
  const std::string filepath = "assets/scenes/test/test_point_light_roundtrip.json";

  Scene scene;
  auto entity = scene.createEntity();
  auto& pl = scene.getRegistry().emplace<PointLightComponent>(entity);
  pl.intensity = 5.5f;
  pl.color = glm::vec3(1.0f, 0.5f, 0.25f);
  pl.radius = 30.0f;
  pl.bake = true;
  pl.lightType = LightMobility::Dynamic;

  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto view = scene2.getRegistry().view<PointLightComponent>();
  ASSERT_EQ(std::distance(view.begin(), view.end()), 1);

  for (auto e : view) {
    auto& p2 = scene2.getRegistry().get<PointLightComponent>(e);
    EXPECT_TRUE(p2.bake);
    EXPECT_EQ(p2.lightType, LightMobility::Dynamic);
    EXPECT_FLOAT_EQ(p2.intensity, 5.5f);
    EXPECT_FLOAT_EQ(p2.radius, 30.0f);
    EXPECT_FLOAT_EQ(p2.color.x, 1.0f);
    EXPECT_FLOAT_EQ(p2.color.y, 0.5f);
    EXPECT_FLOAT_EQ(p2.color.z, 0.25f);
  }
}

// =============================================================================
// SpotLight Roundtrip Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenSpotLightWithAllFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
  const std::string filepath = "assets/scenes/test/test_spot_light_roundtrip.json";

  Scene scene;
  auto entity = scene.createEntity();
  auto& sl = scene.getRegistry().emplace<SpotLightComponent>(entity);
  sl.intensity = 3.0f;
  sl.color = glm::vec3(0.8f, 0.9f, 1.0f);
  sl.innerCutoffAngle = 15.0f;
  sl.outerCutoffAngle = 25.0f;
  sl.bake = true;
  sl.lightType = LightMobility::Static;

  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto view = scene2.getRegistry().view<SpotLightComponent>();
  ASSERT_EQ(std::distance(view.begin(), view.end()), 1);

  for (auto e : view) {
    auto& s2 = scene2.getRegistry().get<SpotLightComponent>(e);
    EXPECT_TRUE(s2.bake);
    EXPECT_EQ(s2.lightType, LightMobility::Static);
    EXPECT_FLOAT_EQ(s2.intensity, 3.0f);
    EXPECT_FLOAT_EQ(s2.innerCutoffAngle, 15.0f);
    EXPECT_FLOAT_EQ(s2.outerCutoffAngle, 25.0f);
    EXPECT_FLOAT_EQ(s2.color.x, 0.8f);
  }
}

// =============================================================================
// Empty Scene Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenEmptyScene_WhenSerialized_ThenDeserializesWithDefaultCamera) {
  const std::string filepath = "assets/scenes/test/test_empty_scene.json";

  Scene scene;
  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  // Empty scene file should still parse but might not have "objects"
  serializer2.deserialize(filepath);

  // Should have a default camera created
  auto camView = scene2.getRegistry().view<CameraComponent>();
  EXPECT_GE(std::distance(camView.begin(), camView.end()), 1);
}

// =============================================================================
// Transform Roundtrip Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenEntityWithTransform_WhenSerializedAndDeserialized_ThenTransformPreserved) {
  const std::string filepath = "assets/scenes/test/test_transform_roundtrip.json";

  Scene scene;
  auto entity = scene.createEntity();
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);
  transform.translation = glm::vec3(10.0f, 20.0f, 30.0f);
  transform.rotation = glm::vec3(0.5f, 1.0f, 1.5f);
  transform.scale = glm::vec3(2.0f, 3.0f, 4.0f);

  // Need at least a directional light for the scene to have "objects"
  scene.getRegistry().emplace<DirectionalLightComponent>(entity);

  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto view = scene2.getRegistry().view<TransformComponent, DirectionalLightComponent>();
  ASSERT_GE(std::distance(view.begin(), view.end()), 1);

  for (auto e : view) {
    auto& t2 = scene2.getRegistry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t2.translation.x, 10.0f);
    EXPECT_FLOAT_EQ(t2.translation.y, 20.0f);
    EXPECT_FLOAT_EQ(t2.translation.z, 30.0f);
    EXPECT_FLOAT_EQ(t2.rotation.x, 0.5f);
    EXPECT_FLOAT_EQ(t2.rotation.y, 1.0f);
    EXPECT_FLOAT_EQ(t2.rotation.z, 1.5f);
    EXPECT_FLOAT_EQ(t2.scale.x, 2.0f);
    EXPECT_FLOAT_EQ(t2.scale.y, 3.0f);
    EXPECT_FLOAT_EQ(t2.scale.z, 4.0f);
  }
}

// =============================================================================
// Invalid File Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenNonexistentFile_WhenDeserialized_ThenReturnsFalse) {
  Scene scene;
  SceneSerializer serializer(scene, resourceManager());
  EXPECT_FALSE(serializer.deserialize("assets/scenes/test/nonexistent_file_12345.json"));
}

// =============================================================================
// Multiple Lights Tests
// =============================================================================

TEST_F(SceneSerializerTest, GivenSceneWithMultipleLightTypes_WhenSerializedAndDeserialized_ThenAllLightsPreserved) {
  const std::string filepath = "assets/scenes/test/test_multi_lights.json";

  Scene scene;

  // Add point light
  auto pointEntity = scene.createEntity();
  auto& pl = scene.getRegistry().emplace<PointLightComponent>(pointEntity);
  pl.intensity = 1.0f;
  scene.getRegistry().emplace<TransformComponent>(pointEntity);

  // Add directional light
  auto dirEntity = scene.createEntity();
  auto& dl = scene.getRegistry().emplace<DirectionalLightComponent>(dirEntity);
  dl.intensity = 2.0f;
  scene.getRegistry().emplace<TransformComponent>(dirEntity);

  // Add spot light
  auto spotEntity = scene.createEntity();
  auto& sl = scene.getRegistry().emplace<SpotLightComponent>(spotEntity);
  sl.intensity = 3.0f;
  scene.getRegistry().emplace<TransformComponent>(spotEntity);

  SceneSerializer serializer(scene, resourceManager());
  serializer.serialize(filepath);

  Scene scene2;
  SceneSerializer serializer2(scene2, resourceManager());
  ASSERT_TRUE(serializer2.deserialize(filepath));

  auto pointView = scene2.getRegistry().view<PointLightComponent>();
  auto dirView = scene2.getRegistry().view<DirectionalLightComponent>();
  auto spotView = scene2.getRegistry().view<SpotLightComponent>();

  EXPECT_EQ(std::distance(pointView.begin(), pointView.end()), 1);
  EXPECT_EQ(std::distance(dirView.begin(), dirView.end()), 1);
  EXPECT_EQ(std::distance(spotView.begin(), spotView.end()), 1);
}

TEST_F(SceneSerializerTest, GivenCollisionReproScene_WhenDeserializedAndSimulated_ThenCubeBouncesOffStaticBody) {
  std::string const path = "assets/scenes/test/collision_repro_scene.json";

  if (!std::filesystem::exists(path)) {
    GTEST_SKIP() << "Missing collision repro scene file: " << path;
  }

  Scene scene;
  SceneSerializer serializer(scene, resourceManager());
  ASSERT_TRUE(serializer.deserialize(path));

  entt::entity staticEntity = entt::null;
  entt::entity dynamicEntity = entt::null;

  auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
  for (auto entity : view) {
    auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
    if (rb.isStatic) {
      staticEntity = entity;
    } else if (rb.useGravity) {
      dynamicEntity = entity;
    }
  }

  ASSERT_TRUE(staticEntity != entt::null);
  ASSERT_TRUE(dynamicEntity != entt::null);

  auto& staticTransform = scene.getRegistry().get<TransformComponent>(staticEntity);
  auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
  auto& staticBody = scene.getRegistry().get<RigidBodyComponent>(staticEntity);
  auto& dynamicBody = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);
  auto& staticCollider = scene.getRegistry().get<ColliderComponent>(staticEntity);
  auto& dynamicCollider = scene.getRegistry().get<ColliderComponent>(dynamicEntity);

  // Move both cubes away from the built-in ground plane (Y = 0) so this test
  // validates cube-vs-cube bounce instead of cube-vs-ground response.
  staticTransform.translation.y = 40.0f;
  dynamicTransform.translation.y = 35.0f;
  staticBody.pendingBodyStateOverride = true;
  dynamicBody.pendingBodyStateOverride = true;

  // Force an elastic setup so the bounce signal is deterministic.
  staticBody.restitution = 0.95f;
  dynamicBody.restitution = 0.95f;
  staticBody.friction = 0.0f;
  dynamicBody.friction = 0.0f;
  dynamicBody.useGravity = false;
  dynamicBody.velocity = glm::vec3(0.0f, 10.0f, 0.0f);
  dynamicBody.pendingBodyStateOverride = true;

    printf("[setup] static isStatic=%d mode=%d trigger=%d size=(%.2f,%.2f,%.2f)\\n",
      staticBody.isStatic ? 1 : 0,
      static_cast<int>(staticBody.mode),
      staticCollider.isTrigger ? 1 : 0,
      staticCollider.size.x,
      staticCollider.size.y,
      staticCollider.size.z);
    printf("[setup] dynamic isStatic=%d mode=%d trigger=%d size=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f)\\n",
      dynamicBody.isStatic ? 1 : 0,
      static_cast<int>(dynamicBody.mode),
      dynamicCollider.isTrigger ? 1 : 0,
      dynamicCollider.size.x,
      dynamicCollider.size.y,
      dynamicCollider.size.z,
      dynamicBody.velocity.x,
      dynamicBody.velocity.y,
      dynamicBody.velocity.z);
  float const startY = dynamicTransform.translation.y;

  JoltPhysicsSystem physicsSystem;
  bool sawDownwardVelocity = false;
  bool sawContact = false;
  bool sawUpwardReboundAfterContact = false;
  float previousY = dynamicTransform.translation.y;

  for (int i = 0; i < 120; ++i) {
    physicsSystem.syncToEntities(&scene);
    physicsSystem.update(1.0f / 60.0f);
    physicsSystem.syncToEntities(&scene);

    if (i < 40 || (i >= 20 && i <= 35)) {
      printf("[step %3d] dynY=%.4f  staticY=%.4f  velY=%.4f\n",
             i, dynamicTransform.translation.y, staticTransform.translation.y,
             dynamicBody.velocity.y);
    }

    float const centerSeparationY = dynamicTransform.translation.y - staticTransform.translation.y;
    float const absCenterSeparationY = std::abs(centerSeparationY);
    if (dynamicBody.velocity.y > 0.2f) {
      sawDownwardVelocity = true;
    }
    if (absCenterSeparationY < 1.25f) {
      sawContact = true;
    }
    if (sawContact && dynamicBody.velocity.y < -0.05f) {
      sawUpwardReboundAfterContact = true;
      break;
    }

    if (sawContact && dynamicTransform.translation.y < previousY - 0.001f) {
      sawUpwardReboundAfterContact = true;
      break;
    }

    previousY = dynamicTransform.translation.y;
  }

  // Y+ is downward in this project, so a falling object's Y should increase.
  EXPECT_GT(dynamicTransform.translation.y, startY);

  // Assert a real bounce signature: fall, contact, then rebound upward.
  EXPECT_TRUE(sawDownwardVelocity);
  EXPECT_TRUE(sawContact);
  EXPECT_TRUE(sawUpwardReboundAfterContact);
}
