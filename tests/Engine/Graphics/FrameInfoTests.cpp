#include <gtest/gtest.h>

#include "Engine/Graphics/FrameInfo.hpp"

using namespace engine;

// =============================================================================
// PointLight Tests
// =============================================================================

TEST(PointLight, GivenDefaultConstructed_WhenInspected_ThenValuesAreZeroOrDefault) {
  PointLight light{};

  EXPECT_FLOAT_EQ(light.positionRadius2.x, 0.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.y, 0.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.z, 0.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.w, 0.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.x, 0.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.y, 0.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.z, 0.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.w, 0.0f);
}

TEST(PointLight, GivenInitializedLight_WhenValuesSet_ThenValuesAreCorrect) {
  PointLight light;
  light.positionRadius2 = glm::vec4(1.0f, 2.0f, 3.0f, 100.0f);
  light.colorIntensity = glm::vec4(1.0f, 0.5f, 0.0f, 10.0f);  // RGB + intensity in w

  EXPECT_FLOAT_EQ(light.positionRadius2.x, 1.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.y, 2.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.z, 3.0f);
  EXPECT_FLOAT_EQ(light.positionRadius2.w, 100.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.w, 10.0f);  // intensity
}

TEST(PointLight, GivenStructLayout_WhenSizeChecked_ThenIsExpectedForGPU) {
  EXPECT_EQ(sizeof(PointLight), 32);
}

// =============================================================================
// DirectionalLight Tests
// =============================================================================

TEST(DirectionalLight, GivenDefaultConstructed_WhenInspected_ThenValuesAreZero) {
  DirectionalLight light{};

  EXPECT_FLOAT_EQ(light.direction.x, 0.0f);
  EXPECT_FLOAT_EQ(light.direction.y, 0.0f);
  EXPECT_FLOAT_EQ(light.direction.z, 0.0f);
  EXPECT_FLOAT_EQ(light.color.x, 0.0f);
  EXPECT_FLOAT_EQ(light.color.y, 0.0f);
  EXPECT_FLOAT_EQ(light.color.z, 0.0f);
}

TEST(DirectionalLight, GivenInitializedLight_WhenValuesSet_ThenDirectionIsNormalized) {
  DirectionalLight light;
  light.direction = glm::normalize(glm::vec4(1.0f, -1.0f, 0.0f, 0.0f));
  light.color = glm::vec4(1.0f, 1.0f, 1.0f, 2.5f);  // White light at 2.5 intensity

  // Direction should be normalized
  float length = glm::length(glm::vec3(light.direction));
  EXPECT_NEAR(length, 1.0f, 0.0001f);
}

TEST(DirectionalLight, GivenStructLayout_WhenSizeChecked_ThenIsExpectedForGPU) {
  // DirectionalLight should be 32 bytes (2 vec4s)
  EXPECT_EQ(sizeof(DirectionalLight), 32);
}

// =============================================================================
// SpotLight Tests
// =============================================================================

TEST(SpotLight, GivenDefaultConstructed_WhenInspected_ThenValuesAreZeroOrDefault) {
  SpotLight light{};

  EXPECT_FLOAT_EQ(light.positionRadius2.x, 0.0f);
  EXPECT_FLOAT_EQ(light.directionInner.x, 0.0f);
  EXPECT_FLOAT_EQ(light.colorIntensity.x, 0.0f);
  EXPECT_FLOAT_EQ(light.attenOuter.x, 0.0f);
}

TEST(SpotLight, GivenInitializedSpotLight_WhenCutoffSet_ThenInnerIsGreaterThanOuter) {
  SpotLight light;
  // Inner cutoff (in direction.w) should be smaller angle = larger cosine
  // Outer cutoff should be larger angle = smaller cosine
  float innerCutoffAngle = glm::radians(15.0f);
  float outerCutoffAngle = glm::radians(25.0f);

  light.directionInner.w = glm::cos(innerCutoffAngle);
  light.attenOuter.x = glm::cos(outerCutoffAngle);

  // cos(smaller angle) > cos(larger angle)
  EXPECT_GT(light.directionInner.w, light.attenOuter.x);
}

TEST(SpotLight, GivenStructLayout_WhenSizeChecked_ThenIsExpectedForGPU) {
  EXPECT_EQ(sizeof(SpotLight), 64);
}

// =============================================================================
// GlobalUbo Alignment Tests
// =============================================================================

TEST(GlobalUbo, GivenGlobalUboStruct_WhenAlignmentChecked_ThenMeetsStd140Requirements) {
  // Verify struct meets std140 alignment requirements
  GlobalUbo ubo{};

  // frustumPlanes must be 16-byte aligned
  EXPECT_EQ(offsetof(GlobalUbo, frustumPlanes) % 16, 0);

  // Overall struct size must be multiple of 16
  EXPECT_EQ(sizeof(GlobalUbo) % 16, 0);
}

TEST(GlobalUbo, GivenDefaultConstructed_WhenInspected_ThenHasReasonableDefaults) {
  GlobalUbo ubo{};

  // Matrices should be identity
  EXPECT_EQ(ubo.projection, glm::mat4(1.0f));
  EXPECT_EQ(ubo.view, glm::mat4(1.0f));
  EXPECT_EQ(ubo.invProjection, glm::mat4(1.0f));
  EXPECT_EQ(ubo.invView, glm::mat4(1.0f));

  // Light counts should be zero
  EXPECT_EQ(ubo.pointLightCount, 0);
  EXPECT_EQ(ubo.directionalLightCount, 0);
  EXPECT_EQ(ubo.spotLightCount, 0);
  EXPECT_EQ(ubo.shadowLightCount, 0);

  // Debug mode off
  EXPECT_EQ(ubo.debugMode, 0);
}

TEST(GlobalUbo, GivenUbo_WhenLightCountsModified_ThenCorrectValuesStored) {
  GlobalUbo ubo{};

  ubo.pointLightCount = 10;
  ubo.directionalLightCount = 2;
  ubo.spotLightCount = 5;
  ubo.shadowLightCount = 4;
  ubo.cubeShadowLightCount = 3;

  EXPECT_EQ(ubo.pointLightCount, 10);
  EXPECT_EQ(ubo.directionalLightCount, 2);
  EXPECT_EQ(ubo.spotLightCount, 5);
  EXPECT_EQ(ubo.shadowLightCount, 4);
  EXPECT_EQ(ubo.cubeShadowLightCount, 3);
}

TEST(GlobalUbo, GivenUbo_WhenFrustumPlanesSet_ThenCanBeRetrieved) {
  GlobalUbo ubo{};

  // Set frustum planes (Left, Right, Bottom, Top, Near, Far)
  for (int i = 0; i < 6; ++i) {
    ubo.frustumPlanes[i] = glm::vec4(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
  }

  for (int i = 0; i < 6; ++i) {
    EXPECT_FLOAT_EQ(ubo.frustumPlanes[i].x, static_cast<float>(i));
  }
}

// =============================================================================
// maxShadowLightCount Constant Tests
// =============================================================================

TEST(FrameInfoConstants, GivenMaxShadowLightCount_WhenChecked_ThenHasExpectedValue) {
  // Verify the constant is set to a reasonable value
  EXPECT_EQ(maxShadowLightCount, 8);

  // GlobalUbo should have arrays sized to this constant
  GlobalUbo ubo{};
  EXPECT_EQ(sizeof(ubo.lightSpaceMatrices) / sizeof(glm::mat4), maxShadowLightCount);
}
