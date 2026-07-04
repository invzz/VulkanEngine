#include <gtest/gtest.h>

#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "../../fixtures/FrameInfoFixture.hpp"
using namespace engine;
namespace {
    class ShadowSystemTest : public engine::test::FrameInfoFixture {};
}  // namespace
TEST(ShadowSettings, GivenDefaultConstruction_WhenInspected_ThenValuesAreReasonableDefaults) {
    ShadowSettings settings;
    EXPECT_FALSE(settings.enableShadowCulling);
    EXPECT_FLOAT_EQ(settings.pointLightDefaultRange, 25.0f);
    EXPECT_FLOAT_EQ(settings.spotLightDefaultRange, 50.0f);
}
TEST_F(ShadowSystemTest, GivenDefaultShadowMapSize_WhenConstructed_ThenInitialCountsAreZero) {
    ShadowSystem shadowSystem(device());
    EXPECT_EQ(shadowSystem.getShadowLightCount(), 0);
    EXPECT_EQ(shadowSystem.getCubeShadowLightCount(), 0);
}
TEST_F(ShadowSystemTest, GivenNoLights_WhenRenderingShadowMaps_ThenNoShadowMapsAreRendered) {
    ShadowSystem   shadowSystem(device());
    Scene          scene;
    Camera         camera;
    ShadowSettings settings;
    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));
    VkCommandBuffer cmd       = device().beginSingleTimeCommands();
    FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
    frameInfo.commandBuffer   = cmd;
    shadowSystem.renderShadowMaps(frameInfo, settings);
    device().endSingleTimeCommands(cmd);
    EXPECT_EQ(shadowSystem.getShadowLightCount(), 0);
    EXPECT_EQ(shadowSystem.getCubeShadowLightCount(), 0);
}
TEST_F(ShadowSystemTest, GivenPointLight_WhenRenderingShadowMaps_ThenCubeShadowMapIsRendered) {
    ShadowSystem   shadowSystem(device());
    Scene          scene;
    Camera         camera;
    ShadowSettings settings;
    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));
    auto  entity              = scene.createEntity();
    auto& pointLight          = scene.getRegistry().emplace<PointLightComponent>(entity);
    auto& transform           = scene.getRegistry().emplace<TransformComponent>(entity);
    pointLight.color          = glm::vec3(1.0f);
    pointLight.intensity      = 1.0f;
    transform.translation     = glm::vec3(0.0f, 5.0f, 0.0f);
    VkCommandBuffer cmd       = device().beginSingleTimeCommands();
    FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
    frameInfo.commandBuffer   = cmd;
    shadowSystem.renderShadowMaps(frameInfo, settings);
    device().endSingleTimeCommands(cmd);
    EXPECT_EQ(shadowSystem.getCubeShadowLightCount(), 1);
    EXPECT_FLOAT_EQ(shadowSystem.getPointLightRange(0), settings.pointLightDefaultRange);
}
TEST_F(ShadowSystemTest, GivenNewShadowSystem_WhenGettingDescriptorInfo_ThenAllShadowMapsHaveValidDescriptors) {
    ShadowSystem shadowSystem(device());
    for (int i = 0; i < ShadowSystem::MAX_SHADOW_MAPS; ++i) {
        VkDescriptorImageInfo info = shadowSystem.getShadowMapDescriptorInfo(i);
        EXPECT_NE(info.sampler, VK_NULL_HANDLE) << "Shadow map " << i << " sampler is null";
        EXPECT_NE(info.imageView, VK_NULL_HANDLE) << "Shadow map " << i << " imageView is null";
    }
}
TEST_F(ShadowSystemTest, GivenNewShadowSystem_WhenGettingCubeDescriptorInfo_ThenAllCubeShadowMapsHaveValidDescriptors) {
    ShadowSystem shadowSystem(device());
    for (int i = 0; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; ++i) {
        VkDescriptorImageInfo info = shadowSystem.getCubeShadowMapDescriptorInfo(i);
        EXPECT_NE(info.sampler, VK_NULL_HANDLE) << "Cube shadow map " << i << " sampler is null";
        EXPECT_NE(info.imageView, VK_NULL_HANDLE) << "Cube shadow map " << i << " imageView is null";
    }
}
