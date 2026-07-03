/**
 * @file LightComponentTests.cpp
 * @brief Unit tests for light components and LightCommon utilities
 *
 * Tests default values, properties, and serialization helpers for all light types.
 */

#include <glm/glm.hpp>

#include <gtest/gtest.h>

#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/LightCommon.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"

namespace {

    constexpr float EPSILON = 1e-6f;

    TEST(LightMobility, GivenStaticEnum_WhenCastToInt_ThenValueIsZero) {
        EXPECT_EQ(static_cast<int>(engine::LightMobility::Static), 0);
    }

    TEST(LightMobility, GivenDynamicEnum_WhenCastToInt_ThenValueIsOne) {
        EXPECT_EQ(static_cast<int>(engine::LightMobility::Dynamic), 1);
    }

    TEST(LightCommon, GivenStaticMobility_WhenToString_ThenReturnsStatic) {
        EXPECT_STREQ(engine::to_string(engine::LightMobility::Static), "static");
    }

    TEST(LightCommon, GivenDynamicMobility_WhenToString_ThenReturnsDynamic) {
        EXPECT_STREQ(engine::to_string(engine::LightMobility::Dynamic), "dynamic");
    }

    TEST(LightCommon, GivenStaticString_WhenParsed_ThenReturnsStaticEnum) {
        EXPECT_EQ(engine::mobility_from_string("static"), engine::LightMobility::Static);
    }

    TEST(LightCommon, GivenDynamicString_WhenParsed_ThenReturnsDynamicEnum) {
        EXPECT_EQ(engine::mobility_from_string("dynamic"), engine::LightMobility::Dynamic);
    }

    TEST(LightCommon, GivenEmptyString_WhenParsed_ThenDefaultsToStatic) {
        EXPECT_EQ(engine::mobility_from_string(""), engine::LightMobility::Static);
    }

    TEST(LightCommon, GivenUnknownString_WhenParsed_ThenDefaultsToStatic) {
        EXPECT_EQ(engine::mobility_from_string("unknown"), engine::LightMobility::Static);
        EXPECT_EQ(engine::mobility_from_string("DYNAMIC"), engine::LightMobility::Static);
        EXPECT_EQ(engine::mobility_from_string("stationary"), engine::LightMobility::Static);
    }

    TEST(LightCommon, GivenRoundtrip_WhenStaticConvertedBothWays_ThenMatches) {
        auto original = engine::LightMobility::Static;
        auto str      = engine::to_string(original);
        auto back     = engine::mobility_from_string(str);
        EXPECT_EQ(original, back);
    }

    TEST(LightCommon, GivenRoundtrip_WhenDynamicConvertedBothWays_ThenMatches) {
        auto original = engine::LightMobility::Dynamic;
        auto str      = engine::to_string(original);
        auto back     = engine::mobility_from_string(str);
        EXPECT_EQ(original, back);
    }

    TEST(PointLightComponent, GivenDefaultConstruction_WhenInspected_ThenIntensityIsOne) {
        engine::PointLightComponent light;
        EXPECT_NEAR(light.intensity, 1.0f, EPSILON);
    }

    TEST(PointLightComponent, GivenDefaultConstruction_WhenInspected_ThenColorIsWhite) {
        engine::PointLightComponent light;
        EXPECT_NEAR(light.color.r, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.g, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.b, 1.0f, EPSILON);
    }

    TEST(PointLightComponent, GivenDefaultConstruction_WhenInspected_ThenRadiusIs15) {
        engine::PointLightComponent light;
        EXPECT_NEAR(light.radius, 15.0f, EPSILON);
    }

    TEST(PointLightComponent, GivenDefaultConstruction_WhenInspected_ThenBakeIsFalse) {
        engine::PointLightComponent light;
        EXPECT_FALSE(light.bake);
    }

    TEST(PointLightComponent, GivenDefaultConstruction_WhenInspected_ThenLightTypeIsStatic) {
        engine::PointLightComponent light;
        EXPECT_EQ(light.lightType, engine::LightMobility::Static);
    }

    TEST(PointLightComponent, GivenLight_WhenPropertiesSet_ThenValuesAreStored) {
        engine::PointLightComponent light;
        light.intensity = 5.0f;
        light.color     = glm::vec3(1.0f, 0.5f, 0.0f);
        light.radius    = 25.0f;
        light.bake      = true;
        light.lightType = engine::LightMobility::Dynamic;

        EXPECT_NEAR(light.intensity, 5.0f, EPSILON);
        EXPECT_NEAR(light.color.r, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.g, 0.5f, EPSILON);
        EXPECT_NEAR(light.color.b, 0.0f, EPSILON);
        EXPECT_NEAR(light.radius, 25.0f, EPSILON);
        EXPECT_TRUE(light.bake);
        EXPECT_EQ(light.lightType, engine::LightMobility::Dynamic);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenIntensityIsOne) {
        engine::DirectionalLightComponent light;
        EXPECT_NEAR(light.intensity, 1.0f, EPSILON);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenColorIsWhite) {
        engine::DirectionalLightComponent light;
        EXPECT_NEAR(light.color.r, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.g, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.b, 1.0f, EPSILON);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenUseTargetPointIsFalse) {
        engine::DirectionalLightComponent light;
        EXPECT_FALSE(light.useTargetPoint);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenTargetPointIsZero) {
        engine::DirectionalLightComponent light;
        EXPECT_NEAR(light.targetPoint.x, 0.0f, EPSILON);
        EXPECT_NEAR(light.targetPoint.y, 0.0f, EPSILON);
        EXPECT_NEAR(light.targetPoint.z, 0.0f, EPSILON);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenBakeIsFalse) {
        engine::DirectionalLightComponent light;
        EXPECT_FALSE(light.bake);
    }

    TEST(DirectionalLightComponent, GivenDefaultConstruction_WhenInspected_ThenLightTypeIsStatic) {
        engine::DirectionalLightComponent light;
        EXPECT_EQ(light.lightType, engine::LightMobility::Static);
    }

    TEST(DirectionalLightComponent, GivenLight_WhenPropertiesSet_ThenValuesAreStored) {
        engine::DirectionalLightComponent light;
        light.intensity      = 2.5f;
        light.color          = glm::vec3(1.0f, 0.9f, 0.8f);
        light.useTargetPoint = true;
        light.targetPoint    = glm::vec3(10.0f, -5.0f, 0.0f);
        light.bake           = true;
        light.lightType      = engine::LightMobility::Dynamic;

        EXPECT_NEAR(light.intensity, 2.5f, EPSILON);
        EXPECT_TRUE(light.useTargetPoint);
        EXPECT_NEAR(light.targetPoint.x, 10.0f, EPSILON);
        EXPECT_NEAR(light.targetPoint.y, -5.0f, EPSILON);
        EXPECT_TRUE(light.bake);
        EXPECT_EQ(light.lightType, engine::LightMobility::Dynamic);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenIntensityIsOne) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.intensity, 1.0f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenColorIsWhite) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.color.r, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.g, 1.0f, EPSILON);
        EXPECT_NEAR(light.color.b, 1.0f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenInnerCutoffIs12Point5) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.innerCutoffAngle, 12.5f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenOuterCutoffIs17Point5) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.outerCutoffAngle, 17.5f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenAttenuationIsDefault) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.constantAttenuation, 1.0f, EPSILON);
        EXPECT_NEAR(light.linearAttenuation, 0.09f, EPSILON);
        EXPECT_NEAR(light.quadraticAttenuation, 0.032f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenUseTargetPointIsFalse) {
        engine::SpotLightComponent light;
        EXPECT_FALSE(light.useTargetPoint);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenTargetPointIsZero) {
        engine::SpotLightComponent light;
        EXPECT_NEAR(light.targetPoint.x, 0.0f, EPSILON);
        EXPECT_NEAR(light.targetPoint.y, 0.0f, EPSILON);
        EXPECT_NEAR(light.targetPoint.z, 0.0f, EPSILON);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenBakeIsFalse) {
        engine::SpotLightComponent light;
        EXPECT_FALSE(light.bake);
    }

    TEST(SpotLightComponent, GivenDefaultConstruction_WhenInspected_ThenLightTypeIsStatic) {
        engine::SpotLightComponent light;
        EXPECT_EQ(light.lightType, engine::LightMobility::Static);
    }

    TEST(SpotLightComponent, GivenLight_WhenPropertiesSet_ThenValuesAreStored) {
        engine::SpotLightComponent light;
        light.intensity            = 3.0f;
        light.color                = glm::vec3(0.8f, 0.9f, 1.0f);
        light.innerCutoffAngle     = 10.0f;
        light.outerCutoffAngle     = 20.0f;
        light.constantAttenuation  = 1.0f;
        light.linearAttenuation    = 0.05f;
        light.quadraticAttenuation = 0.01f;
        light.useTargetPoint       = true;
        light.targetPoint          = glm::vec3(0.0f, -10.0f, 0.0f);
        light.bake                 = true;
        light.lightType            = engine::LightMobility::Dynamic;

        EXPECT_NEAR(light.intensity, 3.0f, EPSILON);
        EXPECT_NEAR(light.innerCutoffAngle, 10.0f, EPSILON);
        EXPECT_NEAR(light.outerCutoffAngle, 20.0f, EPSILON);
        EXPECT_NEAR(light.linearAttenuation, 0.05f, EPSILON);
        EXPECT_NEAR(light.quadraticAttenuation, 0.01f, EPSILON);
        EXPECT_TRUE(light.useTargetPoint);
        EXPECT_NEAR(light.targetPoint.y, -10.0f, EPSILON);
        EXPECT_TRUE(light.bake);
        EXPECT_EQ(light.lightType, engine::LightMobility::Dynamic);
    }

    TEST(SpotLightComponent, GivenSpotLight_WhenInnerGreaterThanOuter_ThenConfigurationIsAllowed) {
        engine::SpotLightComponent light;
        light.innerCutoffAngle = 30.0f;
        light.outerCutoffAngle = 15.0f;

        EXPECT_NEAR(light.innerCutoffAngle, 30.0f, EPSILON);
        EXPECT_NEAR(light.outerCutoffAngle, 15.0f, EPSILON);
    }

    TEST(PointLightComponent, GivenOriginal_WhenCopyConstructed_ThenValuesMatch) {
        engine::PointLightComponent original;
        original.intensity = 2.0f;
        original.color     = glm::vec3(1.0f, 0.0f, 0.0f);
        original.radius    = 30.0f;
        original.bake      = true;
        original.lightType = engine::LightMobility::Dynamic;

        engine::PointLightComponent copy = original;

        EXPECT_NEAR(copy.intensity, 2.0f, EPSILON);
        EXPECT_NEAR(copy.color.r, 1.0f, EPSILON);
        EXPECT_NEAR(copy.radius, 30.0f, EPSILON);
        EXPECT_TRUE(copy.bake);
        EXPECT_EQ(copy.lightType, engine::LightMobility::Dynamic);
    }

    TEST(DirectionalLightComponent, GivenOriginal_WhenCopyConstructed_ThenValuesMatch) {
        engine::DirectionalLightComponent original;
        original.intensity      = 1.5f;
        original.useTargetPoint = true;
        original.targetPoint    = glm::vec3(5.0f, 5.0f, 5.0f);
        original.bake           = true;

        engine::DirectionalLightComponent copy = original;

        EXPECT_NEAR(copy.intensity, 1.5f, EPSILON);
        EXPECT_TRUE(copy.useTargetPoint);
        EXPECT_NEAR(copy.targetPoint.x, 5.0f, EPSILON);
        EXPECT_TRUE(copy.bake);
    }

    TEST(SpotLightComponent, GivenOriginal_WhenCopyConstructed_ThenValuesMatch) {
        engine::SpotLightComponent original;
        original.intensity        = 4.0f;
        original.innerCutoffAngle = 8.0f;
        original.outerCutoffAngle = 25.0f;
        original.bake             = true;

        engine::SpotLightComponent copy = original;

        EXPECT_NEAR(copy.intensity, 4.0f, EPSILON);
        EXPECT_NEAR(copy.innerCutoffAngle, 8.0f, EPSILON);
        EXPECT_NEAR(copy.outerCutoffAngle, 25.0f, EPSILON);
        EXPECT_TRUE(copy.bake);
    }

}  // namespace
