#include <gtest/gtest.h>

#include <cmath>

#include "../../fixtures/FrameInfoFixture.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

using namespace engine;

// Use FrameInfoFixture which provides Device + makeFrameInfo helper
class ShadowSystemTest : public engine::test::FrameInfoFixture
{};

// =============================================================================
// ShadowSettings Tests
// =============================================================================

TEST(ShadowSettings, GivenDefaultConstruction_WhenInspected_ThenValuesAreReasonableDefaults)
{
  ShadowSettings settings;

  EXPECT_FLOAT_EQ(settings.shadowDistance, 100.0f);
  EXPECT_FLOAT_EQ(settings.cascadeLambda, 0.85f);
  EXPECT_FLOAT_EQ(settings.cascadeOverlap, 0.3f);
  EXPECT_FLOAT_EQ(settings.cascadeBlendWidth, 0.2f);
  EXPECT_FALSE(settings.debugVisualization);
}

TEST(ShadowSettings, GivenCustomValues_WhenAssigned_ThenValuesAreStored)
{
  ShadowSettings settings;
  settings.shadowDistance     = 200.0f;
  settings.cascadeLambda      = 0.5f;
  settings.cascadeOverlap     = 0.1f;
  settings.cascadeBlendWidth  = 0.15f;
  settings.debugVisualization = true;

  EXPECT_FLOAT_EQ(settings.shadowDistance, 200.0f);
  EXPECT_FLOAT_EQ(settings.cascadeLambda, 0.5f);
  EXPECT_FLOAT_EQ(settings.cascadeOverlap, 0.1f);
  EXPECT_FLOAT_EQ(settings.cascadeBlendWidth, 0.15f);
  EXPECT_TRUE(settings.debugVisualization);
}

// =============================================================================
// ShadowSystem Construction Tests
// =============================================================================

TEST_F(ShadowSystemTest, GivenDefaultShadowMapSize_WhenConstructed_ThenInitialCountsAreZero)
{
  ShadowSystem shadowSystem(device());

  EXPECT_EQ(shadowSystem.getShadowLightCount(), 0);
  EXPECT_EQ(shadowSystem.getCubeShadowLightCount(), 0);
  EXPECT_EQ(shadowSystem.getDirectionalCascadeCount(), 0);
  EXPECT_EQ(shadowSystem.getDirectionalCascadeBaseIndex(), 0);
}

TEST_F(ShadowSystemTest, GivenCustomShadowMapSize_WhenConstructed_ThenInitialCountsAreZero)
{
  ShadowSystem shadowSystem(device(), 1024);

  EXPECT_EQ(shadowSystem.getShadowLightCount(), 0);
  EXPECT_EQ(shadowSystem.getDirectionalCascadeCount(), 0);
}

TEST_F(ShadowSystemTest, GivenShadowSystem_WhenCheckingConstants_ThenCascadeCountMatchesShaderExpectations)
{
  // Verify the cascade count matches shader expectations (vec4 capacity)
  EXPECT_EQ(ShadowSystem::DIRECTIONAL_CASCADE_COUNT, 4);
}

TEST_F(ShadowSystemTest, GivenShadowSystem_WhenCheckingConstants_ThenMaxShadowMapsAccommodatesAllTypes)
{
  // MAX_SHADOW_MAPS should accommodate all cascades plus spot lights
  EXPECT_EQ(ShadowSystem::MAX_SHADOW_MAPS, ShadowSystem::DIRECTIONAL_CASCADE_COUNT + ShadowSystem::MAX_SPOT_SHADOW_MAPS);
}

// =============================================================================
// CSM Cascade Split Tests
// =============================================================================

TEST_F(ShadowSystemTest, GivenNewShadowSystem_WhenGettingSplits_ThenCascadeSplitsAreZero)
{
  ShadowSystem shadowSystem(device());

  glm::vec4 splits = shadowSystem.getDirectionalCascadeSplits();
  EXPECT_FLOAT_EQ(splits.x, 0.0f);
  EXPECT_FLOAT_EQ(splits.y, 0.0f);
  EXPECT_FLOAT_EQ(splits.z, 0.0f);
  EXPECT_FLOAT_EQ(splits.w, 0.0f);
}

TEST_F(ShadowSystemTest, GivenNoLights_WhenRenderingShadowMaps_ThenNoShadowMapsAreRendered)
{
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
  EXPECT_EQ(shadowSystem.getDirectionalCascadeCount(), 0);
}

TEST_F(ShadowSystemTest, GivenDirectionalLight_WhenRenderingShadowMaps_ThenAllCascadesAreRendered)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  VkCommandBuffer cmd       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
  frameInfo.commandBuffer   = cmd;

  shadowSystem.renderShadowMaps(frameInfo, settings);

  device().endSingleTimeCommands(cmd);

  EXPECT_EQ(shadowSystem.getDirectionalCascadeCount(), ShadowSystem::DIRECTIONAL_CASCADE_COUNT);
  EXPECT_EQ(shadowSystem.getShadowLightCount(), ShadowSystem::DIRECTIONAL_CASCADE_COUNT);
  EXPECT_EQ(shadowSystem.getDirectionalCascadeBaseIndex(), 0);

  glm::vec4 splits = shadowSystem.getDirectionalCascadeSplits();
  EXPECT_GT(splits.x, 0.0f);
  EXPECT_GT(splits.y, splits.x);
  EXPECT_GT(splits.z, splits.y);
  EXPECT_GT(splits.w, splits.z);
}

TEST_F(ShadowSystemTest, GivenCustomShadowDistance_WhenRenderingShadowMaps_ThenSplitsRespectDistance)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;
  settings.shadowDistance = 50.0f;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 200.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  VkCommandBuffer cmd       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
  frameInfo.commandBuffer   = cmd;

  shadowSystem.renderShadowMaps(frameInfo, settings);

  device().endSingleTimeCommands(cmd);

  glm::vec4 splits = shadowSystem.getDirectionalCascadeSplits();
  EXPECT_LE(splits.w, settings.shadowDistance + 1.0f);
}

TEST_F(ShadowSystemTest, GivenDifferentLambdaValues_WhenRenderingShadowMaps_ThenSplitDistributionChanges)
{
  Scene  scene;
  Camera camera;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  // Uniform distribution (lambda = 0)
  ShadowSystem   uniformSystem(device());
  ShadowSettings uniformSettings;
  uniformSettings.cascadeLambda = 0.0f;

  VkCommandBuffer cmd1       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo1 = makeFrameInfo(camera, &scene);
  frameInfo1.commandBuffer   = cmd1;
  uniformSystem.renderShadowMaps(frameInfo1, uniformSettings);
  device().endSingleTimeCommands(cmd1);
  glm::vec4 uniformSplits = uniformSystem.getDirectionalCascadeSplits();

  // Logarithmic distribution (lambda = 1)
  ShadowSystem   logSystem(device());
  ShadowSettings logSettings;
  logSettings.cascadeLambda = 1.0f;

  VkCommandBuffer cmd2       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo2 = makeFrameInfo(camera, &scene);
  frameInfo2.commandBuffer   = cmd2;
  logSystem.renderShadowMaps(frameInfo2, logSettings);
  device().endSingleTimeCommands(cmd2);
  glm::vec4 logSplits = logSystem.getDirectionalCascadeSplits();

  // With log distribution, early cascades should be smaller
  EXPECT_LT(logSplits.x, uniformSplits.x);
}

// =============================================================================
// Light Space Matrix Tests
// =============================================================================

TEST_F(ShadowSystemTest, GivenDirectionalLight_WhenRenderingShadowMaps_ThenLightSpaceMatricesAreValid)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  VkCommandBuffer cmd       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
  frameInfo.commandBuffer   = cmd;

  shadowSystem.renderShadowMaps(frameInfo, settings);

  device().endSingleTimeCommands(cmd);

  for (int i = 0; i < shadowSystem.getShadowLightCount(); ++i)
  {
    const glm::mat4& matrix = shadowSystem.getLightSpaceMatrix(i);

    // Matrix should not be identity
    bool isIdentity = true;
    for (int row = 0; row < 4; ++row)
    {
      for (int col = 0; col < 4; ++col)
      {
        float expected = (row == col) ? 1.0f : 0.0f;
        if (std::abs(matrix[col][row] - expected) > 1e-5f)
        {
          isIdentity = false;
          break;
        }
      }
    }
    EXPECT_FALSE(isIdentity) << "Light space matrix " << i << " should not be identity";

    // Matrix should not contain NaN or Inf
    for (int row = 0; row < 4; ++row)
    {
      for (int col = 0; col < 4; ++col)
      {
        EXPECT_FALSE(std::isnan(matrix[col][row])) << "Light space matrix " << i << " contains NaN";
        EXPECT_FALSE(std::isinf(matrix[col][row])) << "Light space matrix " << i << " contains Inf";
      }
    }
  }
}

// =============================================================================
// Shadow Map Descriptor Tests
// =============================================================================

TEST_F(ShadowSystemTest, GivenNewShadowSystem_WhenGettingDescriptorInfo_ThenAllShadowMapsHaveValidDescriptors)
{
  ShadowSystem shadowSystem(device());

  for (int i = 0; i < ShadowSystem::MAX_SHADOW_MAPS; ++i)
  {
    VkDescriptorImageInfo info = shadowSystem.getShadowMapDescriptorInfo(i);
    EXPECT_NE(info.sampler, VK_NULL_HANDLE) << "Shadow map " << i << " sampler is null";
    EXPECT_NE(info.imageView, VK_NULL_HANDLE) << "Shadow map " << i << " imageView is null";
  }
}

TEST_F(ShadowSystemTest, GivenNewShadowSystem_WhenGettingCubeDescriptorInfo_ThenAllCubeShadowMapsHaveValidDescriptors)
{
  ShadowSystem shadowSystem(device());

  for (int i = 0; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; ++i)
  {
    VkDescriptorImageInfo info = shadowSystem.getCubeShadowMapDescriptorInfo(i);
    EXPECT_NE(info.sampler, VK_NULL_HANDLE) << "Cube shadow map " << i << " sampler is null";
    EXPECT_NE(info.imageView, VK_NULL_HANDLE) << "Cube shadow map " << i << " imageView is null";
  }
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(ShadowSystemTest, GivenVerySmallShadowDistance_WhenRenderingShadowMaps_ThenNoCrash)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;
  settings.shadowDistance = 1.0f;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  VkCommandBuffer cmd       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
  frameInfo.commandBuffer   = cmd;

  EXPECT_NO_THROW(shadowSystem.renderShadowMaps(frameInfo, settings));

  device().endSingleTimeCommands(cmd);

  EXPECT_EQ(shadowSystem.getDirectionalCascadeCount(), ShadowSystem::DIRECTIONAL_CASCADE_COUNT);
}

TEST_F(ShadowSystemTest, GivenVeryLargeShadowDistance_WhenRenderingShadowMaps_ThenSplitsClampToFarPlane)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;
  settings.shadowDistance = 10000.0f;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  VkCommandBuffer cmd       = device().beginSingleTimeCommands();
  FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
  frameInfo.commandBuffer   = cmd;

  EXPECT_NO_THROW(shadowSystem.renderShadowMaps(frameInfo, settings));

  device().endSingleTimeCommands(cmd);

  glm::vec4 splits = shadowSystem.getDirectionalCascadeSplits();
  EXPECT_LE(splits.w, 100.0f + 1.0f);
}

TEST_F(ShadowSystemTest, GivenExtremeLambdaValues_WhenRenderingShadowMaps_ThenNoCrash)
{
  ShadowSystem   shadowSystem(device());
  Scene          scene;
  Camera         camera;
  ShadowSettings settings;

  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  auto  entity    = scene.createEntity();
  auto& dirLight  = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
  auto& transform = scene.getRegistry().emplace<TransformComponent>(entity);

  dirLight.color     = glm::vec3(1.0f);
  dirLight.intensity = 1.0f;
  transform.rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);

  // Lambda = 0 (fully uniform)
  settings.cascadeLambda = 0.0f;
  {
    VkCommandBuffer cmd       = device().beginSingleTimeCommands();
    FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
    frameInfo.commandBuffer   = cmd;
    EXPECT_NO_THROW(shadowSystem.renderShadowMaps(frameInfo, settings));
    device().endSingleTimeCommands(cmd);
  }

  // Lambda = 1 (fully logarithmic)
  settings.cascadeLambda = 1.0f;
  {
    VkCommandBuffer cmd       = device().beginSingleTimeCommands();
    FrameInfo       frameInfo = makeFrameInfo(camera, &scene);
    frameInfo.commandBuffer   = cmd;
    EXPECT_NO_THROW(shadowSystem.renderShadowMaps(frameInfo, settings));
    device().endSingleTimeCommands(cmd);
  }
}
