#include <filesystem>
#include <gtest/gtest.h>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "../fixtures/SceneFixture.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

class SceneSerializerTest : public engine::test::SceneFixture {};

TEST_F(SceneSerializerTest, GivenDirectionalLightWithBakeFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
    const std::string filepath = "assets/scenes/test/test_scene_bake_roundtrip.json";

    Scene scene;
    auto  entity = scene.createEntity();
    auto& dl     = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
    dl.intensity = 1.23f;
    dl.color     = glm::vec3(0.5f, 0.6f, 0.7f);
    dl.bake      = true;
    dl.lightType = LightMobility::Dynamic;

    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
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

TEST_F(SceneSerializerTest, GivenDemoBakeSceneFile_WhenDeserialized_ThenLightComponentsHaveCorrectBakeFlags) {
    std::string const path = "assets/scenes/test/demo_scene_bake.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing demo scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    auto dirView = scene.getRegistry().view<DirectionalLightComponent>();
    ASSERT_GT(std::distance(dirView.begin(), dirView.end()), 0);
    for (auto e : dirView) {
        auto& dl = scene.getRegistry().get<DirectionalLightComponent>(e);
        EXPECT_TRUE(dl.bake);
        EXPECT_EQ(dl.lightType, LightMobility::Static);
    }

    auto pointView = scene.getRegistry().view<PointLightComponent>();
    ASSERT_GT(std::distance(pointView.begin(), pointView.end()), 0);
    for (auto e : pointView) {
        auto& pl = scene.getRegistry().get<PointLightComponent>(e);
        EXPECT_FALSE(pl.bake);
        EXPECT_EQ(pl.lightType, LightMobility::Dynamic);
    }
}

TEST_F(SceneSerializerTest, GivenPointLightWithAllFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
    const std::string filepath = "assets/scenes/test/test_point_light_roundtrip.json";

    Scene scene;
    auto  entity = scene.createEntity();
    auto& pl     = scene.getRegistry().emplace<PointLightComponent>(entity);
    pl.intensity = 5.5f;
    pl.color     = glm::vec3(1.0f, 0.5f, 0.25f);
    pl.radius    = 30.0f;
    pl.bake      = true;
    pl.lightType = LightMobility::Dynamic;

    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
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

TEST_F(SceneSerializerTest, GivenSpotLightWithAllFields_WhenSerializedAndDeserialized_ThenFieldsArePreserved) {
    const std::string filepath = "assets/scenes/test/test_spot_light_roundtrip.json";

    Scene scene;
    auto  entity        = scene.createEntity();
    auto& sl            = scene.getRegistry().emplace<SpotLightComponent>(entity);
    sl.intensity        = 3.0f;
    sl.color            = glm::vec3(0.8f, 0.9f, 1.0f);
    sl.innerCutoffAngle = 15.0f;
    sl.outerCutoffAngle = 25.0f;
    sl.bake             = true;
    sl.lightType        = LightMobility::Static;

    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
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

TEST_F(SceneSerializerTest, GivenEmptyScene_WhenSerialized_ThenDeserializesWithDefaultCamera) {
    const std::string filepath = "assets/scenes/test/test_empty_scene.json";

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
    SceneSerializer serializer2(scene2, resourceManager());

    serializer2.deserialize(filepath);

    auto camView = scene2.getRegistry().view<CameraComponent>();
    EXPECT_GE(std::distance(camView.begin(), camView.end()), 1);
}

TEST_F(SceneSerializerTest, GivenEntityWithTransform_WhenSerializedAndDeserialized_ThenTransformPreserved) {
    const std::string filepath = "assets/scenes/test/test_transform_roundtrip.json";

    Scene scene;
    auto  entity          = scene.createEntity();
    auto& transform       = scene.getRegistry().emplace<TransformComponent>(entity);
    transform.translation = glm::vec3(10.0f, 20.0f, 30.0f);
    transform.rotation    = glm::vec3(0.5f, 1.0f, 1.5f);
    transform.scale       = glm::vec3(2.0f, 3.0f, 4.0f);

    scene.getRegistry().emplace<DirectionalLightComponent>(entity);

    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
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

TEST_F(SceneSerializerTest, GivenNonexistentFile_WhenDeserialized_ThenReturnsFalse) {
    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    EXPECT_FALSE(serializer.deserialize("assets/scenes/test/nonexistent_file_12345.json"));
}

TEST_F(SceneSerializerTest, GivenSceneWithMultipleLightTypes_WhenSerializedAndDeserialized_ThenAllLightsPreserved) {
    const std::string filepath = "assets/scenes/test/test_multi_lights.json";

    Scene scene;

    auto  pointEntity = scene.createEntity();
    auto& pl          = scene.getRegistry().emplace<PointLightComponent>(pointEntity);
    pl.intensity      = 1.0f;
    scene.getRegistry().emplace<TransformComponent>(pointEntity);

    auto  dirEntity = scene.createEntity();
    auto& dl        = scene.getRegistry().emplace<DirectionalLightComponent>(dirEntity);
    dl.intensity    = 2.0f;
    scene.getRegistry().emplace<TransformComponent>(dirEntity);

    auto  spotEntity = scene.createEntity();
    auto& sl         = scene.getRegistry().emplace<SpotLightComponent>(spotEntity);
    sl.intensity     = 3.0f;
    scene.getRegistry().emplace<TransformComponent>(spotEntity);

    SceneSerializer serializer(scene, resourceManager());
    serializer.serialize(filepath);

    Scene           scene2;
    SceneSerializer serializer2(scene2, resourceManager());
    ASSERT_TRUE(serializer2.deserialize(filepath));

    auto pointView = scene2.getRegistry().view<PointLightComponent>();
    auto dirView   = scene2.getRegistry().view<DirectionalLightComponent>();
    auto spotView  = scene2.getRegistry().view<SpotLightComponent>();

    EXPECT_EQ(std::distance(pointView.begin(), pointView.end()), 1);
    EXPECT_EQ(std::distance(dirView.begin(), dirView.end()), 1);
    EXPECT_EQ(std::distance(spotView.begin(), spotView.end()), 1);
}
