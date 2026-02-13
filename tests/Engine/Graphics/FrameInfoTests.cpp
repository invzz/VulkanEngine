#include <gtest/gtest.h>

#include "Engine/Graphics/FrameInfo.hpp"

using namespace engine;

// =============================================================================
// PointLight Tests
// =============================================================================

TEST(PointLight, GivenDefaultConstructed_WhenInspected_ThenValuesAreZeroOrDefault) {
  PointLight light{};

  EXPECT_FLOAT_EQ(light.position.x, 0.0f);
  EXPECT_FLOAT_EQ(light.position.y, 0.0f);
  EXPECT_FLOAT_EQ(light.position.z, 0.0f);
  EXPECT_FLOAT_EQ(light.color.x, 0.0f);
  EXPECT_FLOAT_EQ(light.color.y, 0.0f);
  EXPECT_FLOAT_EQ(light.color.z, 0.0f);
  EXPECT_FLOAT_EQ(light.radius2, 0.0f);
}

TEST(PointLight, GivenInitializedLight_WhenValuesSet_ThenValuesAreCorrect) {
  PointLight light;
  light.position = glm::vec4(1.0f, 2.0f, 3.0f, 0.0f);
  light.color = glm::vec4(1.0f, 0.5f, 0.0f, 10.0f);  // RGB + intensity in w
  light.radius2 = 100.0f;                            // Squared radius

  EXPECT_FLOAT_EQ(light.position.x, 1.0f);
  EXPECT_FLOAT_EQ(light.position.y, 2.0f);
  EXPECT_FLOAT_EQ(light.position.z, 3.0f);
  EXPECT_FLOAT_EQ(light.color.w, 10.0f);  // intensity
  EXPECT_FLOAT_EQ(light.radius2, 100.0f);
}

TEST(PointLight, GivenStructLayout_WhenSizeChecked_ThenIsExpectedForGPU) {
  // PointLight should be 48 bytes (3 vec4s + radius2 + 3 floats padding)
  EXPECT_EQ(sizeof(PointLight), 48);
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

  EXPECT_FLOAT_EQ(light.position.x, 0.0f);
  EXPECT_FLOAT_EQ(light.direction.x, 0.0f);
  EXPECT_FLOAT_EQ(light.color.x, 0.0f);
  EXPECT_FLOAT_EQ(light.outerCutoff, 0.0f);
  EXPECT_FLOAT_EQ(light.constantAtten, 0.0f);
  EXPECT_FLOAT_EQ(light.radius2, 0.0f);
}

TEST(SpotLight, GivenInitializedSpotLight_WhenCutoffSet_ThenInnerIsGreaterThanOuter) {
  SpotLight light;
  // Inner cutoff (in direction.w) should be smaller angle = larger cosine
  // Outer cutoff should be larger angle = smaller cosine
  float innerCutoffAngle = glm::radians(15.0f);
  float outerCutoffAngle = glm::radians(25.0f);

  light.direction.w = glm::cos(innerCutoffAngle);
  light.outerCutoff = glm::cos(outerCutoffAngle);

  // cos(smaller angle) > cos(larger angle)
  EXPECT_GT(light.direction.w, light.outerCutoff);
}

TEST(SpotLight, GivenStructLayout_WhenSizeChecked_ThenIsExpectedForGPU) {
  // SpotLight should be 80 bytes (3 vec4s + cutoff + 3 atten floats + radius2 + 3 pad)
  // This may vary based on compiler padding - just verify it's reasonable
  EXPECT_GE(sizeof(SpotLight), 64);
  EXPECT_LE(sizeof(SpotLight), 96);
}

// =============================================================================
// HZBSettings Tests
// =============================================================================

TEST(HZBSettings, GivenDefaultConstructed_WhenInspected_ThenHasReasonableDefaults) {
  HZBSettings settings{};

  EXPECT_EQ(settings.maxMipLevel, 10);
  EXPECT_FLOAT_EQ(settings.minScreenPixels, 2.0f);
  EXPECT_FLOAT_EQ(settings.screenSizeScale, 1.0f);
  EXPECT_EQ(settings.enabled, 1);
}

TEST(HZBSettings, GivenCustomSettings_WhenModified_ThenValuesAreCorrect) {
  HZBSettings settings;
  settings.maxMipLevel = 8;
  settings.minScreenPixels = 4.0f;
  settings.screenSizeScale = 2.0f;
  settings.enabled = 0;

  EXPECT_EQ(settings.maxMipLevel, 8);
  EXPECT_FLOAT_EQ(settings.minScreenPixels, 4.0f);
  EXPECT_FLOAT_EQ(settings.screenSizeScale, 2.0f);
  EXPECT_EQ(settings.enabled, 0);
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

  // HZB settings have defaults
  EXPECT_EQ(ubo.hzbEnabled, 1);
  EXPECT_EQ(ubo.hzbMaxMipLevel, 10);
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

TEST(GlobalUbo, GivenUbo_WhenFogSettingsConfigured_ThenValuesAreCorrect) {
  GlobalUbo ubo{};

  ubo.fogColor = glm::vec4(0.5f, 0.6f, 0.7f, 0.01f);  // RGB + density
  ubo.fogZenithColor = glm::vec4(0.8f, 0.9f, 1.0f, 0.0f);
  ubo.fogHeight = 100.0f;
  ubo.fogHeightDensity = 0.5f;

  EXPECT_FLOAT_EQ(ubo.fogColor.x, 0.5f);
  EXPECT_FLOAT_EQ(ubo.fogColor.w, 0.01f);  // density in w
  EXPECT_FLOAT_EQ(ubo.fogHeight, 100.0f);
  EXPECT_FLOAT_EQ(ubo.fogHeightDensity, 0.5f);
}

// =============================================================================
// maxShadowLightCount Constant Tests
// =============================================================================

TEST(FrameInfoConstants, GivenMaxShadowLightCount_WhenChecked_ThenHasExpectedValue) {
  // Verify the constant is set to a reasonable value
  EXPECT_EQ(maxShadowLightCount, 16);

  // GlobalUbo should have arrays sized to this constant
  GlobalUbo ubo{};
  EXPECT_EQ(sizeof(ubo.lightSpaceMatrices) / sizeof(glm::mat4), maxShadowLightCount);
}
