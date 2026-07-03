/**
 * @file PBRMaterialTests.cpp
 * @brief Unit tests for the PBRMaterial struct
 *
 * Tests default values, texture map presence checks, and material property ranges.
 */

#include <glm/glm.hpp>

#include <gtest/gtest.h>
#include <memory>

#include "ModelLib/Resources/PBRMaterial.hpp"

namespace {

    constexpr float EPSILON = 1e-6f;

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAlbedoIsWhite) {
        engine::PBRMaterial material;

        EXPECT_NEAR(material.albedo.r, 1.0f, EPSILON);
        EXPECT_NEAR(material.albedo.g, 1.0f, EPSILON);
        EXPECT_NEAR(material.albedo.b, 1.0f, EPSILON);
        EXPECT_NEAR(material.albedo.a, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenMetallicIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.metallic, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenRoughnessIsHalf) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.roughness, 0.5f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAOIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.ao, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAlphaModeIsOpaque) {
        engine::PBRMaterial material;
        EXPECT_EQ(material.alphaMode, engine::AlphaMode::Opaque);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAlphaCutoffIsHalf) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.alphaCutoff, 0.5f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenNotDoubleSided) {
        engine::PBRMaterial material;
        EXPECT_FALSE(material.doubleSided);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenClearcoatIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.clearcoat, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenClearcoatRoughnessIsDefault) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.clearcoatRoughness, 0.03f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAnisotropicIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.anisotropic, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAnisotropicRotationIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.anisotropicRotation, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenTransmissionIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.transmission, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenIORIsOnePointFive) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.ior, 1.5f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenThicknessIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.thickness, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAttenuationColorIsWhite) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.attenuationColor.r, 1.0f, EPSILON);
        EXPECT_NEAR(material.attenuationColor.g, 1.0f, EPSILON);
        EXPECT_NEAR(material.attenuationColor.b, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAttenuationDistanceIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.attenuationDistance, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenIridescenceIsZero) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.iridescence, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenIridescenceIORIsDefault) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.iridescenceIOR, 1.3f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenIridescenceThicknessIsDefault) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.iridescenceThickness, 100.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenEmissiveColorIsBlack) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.emissiveColor.r, 0.0f, EPSILON);
        EXPECT_NEAR(material.emissiveColor.g, 0.0f, EPSILON);
        EXPECT_NEAR(material.emissiveColor.b, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenEmissiveStrengthIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.emissiveStrength, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenMetallicRoughnessTextureNotUsed) {
        engine::PBRMaterial material;
        EXPECT_FALSE(material.useMetallicRoughnessTexture);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenOcclusionRoughnessMetallicNotUsed) {
        engine::PBRMaterial material;
        EXPECT_FALSE(material.useOcclusionRoughnessMetallicTexture);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenSpecularGlossinessNotUsed) {
        engine::PBRMaterial material;
        EXPECT_FALSE(material.useSpecularGlossinessWorkflow);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenSpecularFactorIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.specularFactor.r, 1.0f, EPSILON);
        EXPECT_NEAR(material.specularFactor.g, 1.0f, EPSILON);
        EXPECT_NEAR(material.specularFactor.b, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenGlossinessFactorIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.glossinessFactor, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenUVScaleIsOne) {
        engine::PBRMaterial material;
        EXPECT_NEAR(material.uvScale, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAllMapsAreNull) {
        engine::PBRMaterial material;

        EXPECT_FALSE(material.hasAlbedoMap());
        EXPECT_FALSE(material.hasNormalMap());
        EXPECT_FALSE(material.hasMetallicMap());
        EXPECT_FALSE(material.hasRoughnessMap());
        EXPECT_FALSE(material.hasAOMap());
        EXPECT_FALSE(material.hasEmissiveMap());
        EXPECT_FALSE(material.hasTransmissionMap());
        EXPECT_FALSE(material.hasClearcoatMap());
        EXPECT_FALSE(material.hasClearcoatRoughnessMap());
        EXPECT_FALSE(material.hasClearcoatNormalMap());
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenAlbedoMapIsNull) {
        engine::PBRMaterial material;
        EXPECT_EQ(material.albedoMap, nullptr);
        EXPECT_FALSE(material.hasAlbedoMap());
    }

    TEST(PBRMaterial, GivenDefaultConstruction_WhenInspected_ThenNormalMapIsNull) {
        engine::PBRMaterial material;
        EXPECT_EQ(material.normalMap, nullptr);
        EXPECT_FALSE(material.hasNormalMap());
    }

    TEST(PBRMaterial, GivenMaterial_WhenAlbedoSet_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.albedo = glm::vec4(0.5f, 0.3f, 0.1f, 0.9f);

        EXPECT_NEAR(material.albedo.r, 0.5f, EPSILON);
        EXPECT_NEAR(material.albedo.g, 0.3f, EPSILON);
        EXPECT_NEAR(material.albedo.b, 0.1f, EPSILON);
        EXPECT_NEAR(material.albedo.a, 0.9f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenMetallicSet_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.metallic = 1.0f;

        EXPECT_NEAR(material.metallic, 1.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenRoughnessSet_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.roughness = 0.8f;

        EXPECT_NEAR(material.roughness, 0.8f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenAlphaModeSetToMask_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.alphaMode = engine::AlphaMode::Mask;

        EXPECT_EQ(material.alphaMode, engine::AlphaMode::Mask);
    }

    TEST(PBRMaterial, GivenMaterial_WhenAlphaModeSetToBlend_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.alphaMode = engine::AlphaMode::Blend;

        EXPECT_EQ(material.alphaMode, engine::AlphaMode::Blend);
    }

    TEST(PBRMaterial, GivenMaterial_WhenDoubleSidedSetTrue_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.doubleSided = true;

        EXPECT_TRUE(material.doubleSided);
    }

    TEST(PBRMaterial, GivenMaterial_WhenClearcoatSetToFullStrength_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.clearcoat          = 1.0f;
        material.clearcoatRoughness = 0.1f;

        EXPECT_NEAR(material.clearcoat, 1.0f, EPSILON);
        EXPECT_NEAR(material.clearcoatRoughness, 0.1f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenTransmissionSetToFull_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.transmission = 1.0f;
        material.ior          = 1.33f;

        EXPECT_NEAR(material.transmission, 1.0f, EPSILON);
        EXPECT_NEAR(material.ior, 1.33f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenAttenuationSetForColoredGlass_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.attenuationColor    = glm::vec3(0.8f, 0.2f, 0.1f);
        material.attenuationDistance = 0.5f;

        EXPECT_NEAR(material.attenuationColor.r, 0.8f, EPSILON);
        EXPECT_NEAR(material.attenuationColor.g, 0.2f, EPSILON);
        EXPECT_NEAR(material.attenuationColor.b, 0.1f, EPSILON);
        EXPECT_NEAR(material.attenuationDistance, 0.5f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenIridescenceSetForSoapBubble_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.iridescence          = 1.0f;
        material.iridescenceIOR       = 1.4f;
        material.iridescenceThickness = 300.0f;

        EXPECT_NEAR(material.iridescence, 1.0f, EPSILON);
        EXPECT_NEAR(material.iridescenceIOR, 1.4f, EPSILON);
        EXPECT_NEAR(material.iridescenceThickness, 300.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenEmissiveSetForGlow_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.emissiveColor    = glm::vec3(1.0f, 0.5f, 0.0f);
        material.emissiveStrength = 5.0f;

        EXPECT_NEAR(material.emissiveColor.r, 1.0f, EPSILON);
        EXPECT_NEAR(material.emissiveColor.g, 0.5f, EPSILON);
        EXPECT_NEAR(material.emissiveColor.b, 0.0f, EPSILON);
        EXPECT_NEAR(material.emissiveStrength, 5.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenSpecularGlossinessEnabled_ThenValuesAreStored) {
        engine::PBRMaterial material;
        material.useSpecularGlossinessWorkflow = true;
        material.specularFactor                = glm::vec3(0.04f);
        material.glossinessFactor              = 0.8f;

        EXPECT_TRUE(material.useSpecularGlossinessWorkflow);
        EXPECT_NEAR(material.specularFactor.r, 0.04f, EPSILON);
        EXPECT_NEAR(material.glossinessFactor, 0.8f, EPSILON);
    }

    TEST(PBRMaterial, GivenMaterial_WhenUVScaleSetForTiling_ThenValueIsStored) {
        engine::PBRMaterial material;
        material.uvScale = 4.0f;

        EXPECT_NEAR(material.uvScale, 4.0f, EPSILON);
    }

    TEST(AlphaMode, GivenEnumValues_WhenCast_ThenUnderlyingValuesAreCorrect) {
        EXPECT_EQ(static_cast<std::uint8_t>(engine::AlphaMode::Opaque), 0);
        EXPECT_EQ(static_cast<std::uint8_t>(engine::AlphaMode::Mask), 1);
        EXPECT_EQ(static_cast<std::uint8_t>(engine::AlphaMode::Blend), 2);
    }

    TEST(PBRMaterial, GivenMetalPreset_WhenConfigured_ThenValuesMatchMetalCharacteristics) {
        engine::PBRMaterial metal;
        metal.albedo    = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        metal.metallic  = 1.0f;
        metal.roughness = 0.3f;

        EXPECT_NEAR(metal.metallic, 1.0f, EPSILON);
        EXPECT_LT(metal.roughness, 0.5f);
    }

    TEST(PBRMaterial, GivenDielectricPreset_WhenConfigured_ThenValuesMatchDielectricCharacteristics) {
        engine::PBRMaterial dielectric;
        dielectric.albedo    = glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
        dielectric.metallic  = 0.0f;
        dielectric.roughness = 0.5f;

        EXPECT_NEAR(dielectric.metallic, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenGlassPreset_WhenConfigured_ThenValuesMatchGlassCharacteristics) {
        engine::PBRMaterial glass;
        glass.albedo       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        glass.metallic     = 0.0f;
        glass.roughness    = 0.0f;
        glass.transmission = 1.0f;
        glass.ior          = 1.5f;

        EXPECT_NEAR(glass.transmission, 1.0f, EPSILON);
        EXPECT_NEAR(glass.ior, 1.5f, EPSILON);
        EXPECT_NEAR(glass.roughness, 0.0f, EPSILON);
    }

    TEST(PBRMaterial, GivenCarPaintPreset_WhenConfigured_ThenValuesMatchCarPaintCharacteristics) {
        engine::PBRMaterial carPaint;
        carPaint.albedo             = glm::vec4(0.8f, 0.0f, 0.0f, 1.0f);
        carPaint.metallic           = 0.0f;
        carPaint.roughness          = 0.4f;
        carPaint.clearcoat          = 1.0f;
        carPaint.clearcoatRoughness = 0.03f;

        EXPECT_NEAR(carPaint.clearcoat, 1.0f, EPSILON);
        EXPECT_LT(carPaint.clearcoatRoughness, 0.1f);
    }

}  // namespace
