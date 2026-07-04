/**
 * @file MaterialUniformDataTests.cpp
 * @brief Unit tests for the MaterialUniformData struct
 *
 * Tests default values and packed parameter layout for GPU uniforms.
 */
#include <glm/glm.hpp>

#include <gtest/gtest.h>

#include "ModelLib/Resources/MaterialUniformData.hpp"
namespace {
    constexpr float EPSILON = 1e-6f;
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenAlbedoIsWhite) {
        engine::MaterialUniformData data;
        EXPECT_NEAR(data.albedo.r, 1.0f, EPSILON);
        EXPECT_NEAR(data.albedo.g, 1.0f, EPSILON);
        EXPECT_NEAR(data.albedo.b, 1.0f, EPSILON);
        EXPECT_NEAR(data.albedo.a, 1.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenEmissiveInfoIsDefault) {
        engine::MaterialUniformData data;
        EXPECT_NEAR(data.emissiveInfo.r, 0.0f, EPSILON);
        EXPECT_NEAR(data.emissiveInfo.g, 0.0f, EPSILON);
        EXPECT_NEAR(data.emissiveInfo.b, 0.0f, EPSILON);
        EXPECT_NEAR(data.emissiveInfo.a, 1.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenSpecularGlossinessIsDefault) {
        engine::MaterialUniformData data;
        EXPECT_NEAR(data.specularGlossinessFactor.r, 1.0f, EPSILON);
        EXPECT_NEAR(data.specularGlossinessFactor.g, 1.0f, EPSILON);
        EXPECT_NEAR(data.specularGlossinessFactor.b, 1.0f, EPSILON);
        EXPECT_NEAR(data.specularGlossinessFactor.a, 1.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenAttenuationIsDefault) {
        engine::MaterialUniformData data;
        EXPECT_NEAR(data.attenuationColorAndDist.r, 1.0f, EPSILON);
        EXPECT_NEAR(data.attenuationColorAndDist.g, 1.0f, EPSILON);
        EXPECT_NEAR(data.attenuationColorAndDist.b, 1.0f, EPSILON);
        EXPECT_NEAR(data.attenuationColorAndDist.a, 1.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenParamsMatrixIsZero) {
        engine::MaterialUniformData data;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                EXPECT_NEAR(data.params[col][row], 0.0f, EPSILON);
            }
        }
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenFlagsAndIndicesAreZero) {
        engine::MaterialUniformData data;
        EXPECT_EQ(data.flagsAndIndices0.x, 0u);
        EXPECT_EQ(data.flagsAndIndices0.y, 0u);
        EXPECT_EQ(data.flagsAndIndices0.z, 0u);
        EXPECT_EQ(data.flagsAndIndices0.w, 0u);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices1AreZero) {
        engine::MaterialUniformData data;
        EXPECT_EQ(data.indices1.x, 0u);
        EXPECT_EQ(data.indices1.y, 0u);
        EXPECT_EQ(data.indices1.z, 0u);
        EXPECT_EQ(data.indices1.w, 0u);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices2AreZero) {
        engine::MaterialUniformData data;
        EXPECT_EQ(data.indices2.x, 0u);
        EXPECT_EQ(data.indices2.y, 0u);
        EXPECT_EQ(data.indices2.z, 0u);
        EXPECT_EQ(data.indices2.w, 0u);
    }
    TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices3AreZero) {
        engine::MaterialUniformData data;
        EXPECT_EQ(data.indices3.x, 0u);
        EXPECT_EQ(data.indices3.y, 0u);
        EXPECT_EQ(data.indices3.z, 0u);
        EXPECT_EQ(data.indices3.w, 0u);
    }
    TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenAlbedoIsAtOffset0) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, albedo), 0);
    }
    TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenParamsIsAfterVec4s) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, params), 64);
    }
    TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenFlagsIsAfterParams) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, flagsAndIndices0), 128);
    }
    TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenIndicesAreSequential) {
        size_t flags0Offset   = offsetof(engine::MaterialUniformData, flagsAndIndices0);
        size_t indices1Offset = offsetof(engine::MaterialUniformData, indices1);
        size_t indices2Offset = offsetof(engine::MaterialUniformData, indices2);
        size_t indices3Offset = offsetof(engine::MaterialUniformData, indices3);
        EXPECT_EQ(indices1Offset - flags0Offset, sizeof(glm::uvec4));
        EXPECT_EQ(indices2Offset - indices1Offset, sizeof(glm::uvec4));
        EXPECT_EQ(indices3Offset - indices2Offset, sizeof(glm::uvec4));
    }
    TEST(MaterialUniformData, GivenStruct_WhenSizeChecked_ThenTotalSizeIs192Bytes) {
        EXPECT_EQ(sizeof(engine::MaterialUniformData), 192);
    }
    TEST(MaterialUniformData, GivenParams_WhenColumn0Set_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.params[0] = glm::vec4(0.8f, 0.3f, 1.0f, 0.0f);
        EXPECT_NEAR(data.params[0][0], 0.8f, EPSILON);
        EXPECT_NEAR(data.params[0][1], 0.3f, EPSILON);
        EXPECT_NEAR(data.params[0][2], 1.0f, EPSILON);
        EXPECT_NEAR(data.params[0][3], 0.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenParams_WhenColumn1Set_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.params[1] = glm::vec4(1.0f, 0.03f, 0.5f, 0.25f);
        EXPECT_NEAR(data.params[1][0], 1.0f, EPSILON);
        EXPECT_NEAR(data.params[1][1], 0.03f, EPSILON);
        EXPECT_NEAR(data.params[1][2], 0.5f, EPSILON);
        EXPECT_NEAR(data.params[1][3], 0.25f, EPSILON);
    }
    TEST(MaterialUniformData, GivenParams_WhenColumn2Set_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.params[2] = glm::vec4(0.9f, 1.5f, 0.5f, 1.3f);
        EXPECT_NEAR(data.params[2][0], 0.9f, EPSILON);
        EXPECT_NEAR(data.params[2][1], 1.5f, EPSILON);
        EXPECT_NEAR(data.params[2][2], 0.5f, EPSILON);
        EXPECT_NEAR(data.params[2][3], 1.3f, EPSILON);
    }
    TEST(MaterialUniformData, GivenParams_WhenColumn3Set_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.params[3] = glm::vec4(100.0f, 2.0f, 0.5f, 0.1f);
        EXPECT_NEAR(data.params[3][0], 100.0f, EPSILON);
        EXPECT_NEAR(data.params[3][1], 2.0f, EPSILON);
        EXPECT_NEAR(data.params[3][2], 0.5f, EPSILON);
        EXPECT_NEAR(data.params[3][3], 0.1f, EPSILON);
    }
    TEST(MaterialUniformData, GivenFlagsAndIndices0_WhenSet_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.flagsAndIndices0 = glm::uvec4(0xFF, 1, 5, 6);
        EXPECT_EQ(data.flagsAndIndices0.x, 0xFF);
        EXPECT_EQ(data.flagsAndIndices0.y, 1u);
        EXPECT_EQ(data.flagsAndIndices0.z, 5u);
        EXPECT_EQ(data.flagsAndIndices0.w, 6u);
    }
    TEST(MaterialUniformData, GivenIndices1_WhenSet_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.indices1 = glm::uvec4(7, 8, 9, 10);
        EXPECT_EQ(data.indices1.x, 7u);
        EXPECT_EQ(data.indices1.y, 8u);
        EXPECT_EQ(data.indices1.z, 9u);
        EXPECT_EQ(data.indices1.w, 10u);
    }
    TEST(MaterialUniformData, GivenIndices2_WhenSet_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.indices2 = glm::uvec4(11, 1, 12, 13);
        EXPECT_EQ(data.indices2.x, 11u);
        EXPECT_EQ(data.indices2.y, 1u);
        EXPECT_EQ(data.indices2.z, 12u);
        EXPECT_EQ(data.indices2.w, 13u);
    }
    TEST(MaterialUniformData, GivenIndices3_WhenSet_ThenLayoutMatchesExpected) {
        engine::MaterialUniformData data;
        data.indices3 = glm::uvec4(14, 15, 0, 0);
        EXPECT_EQ(data.indices3.x, 14u);
        EXPECT_EQ(data.indices3.y, 15u);
        EXPECT_EQ(data.indices3.z, 0u);
        EXPECT_EQ(data.indices3.w, 0u);
    }
    TEST(MaterialUniformData, GivenData_WhenAlbedoSetToRed_ThenValueIsStored) {
        engine::MaterialUniformData data;
        data.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        EXPECT_NEAR(data.albedo.r, 1.0f, EPSILON);
        EXPECT_NEAR(data.albedo.g, 0.0f, EPSILON);
        EXPECT_NEAR(data.albedo.b, 0.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenData_WhenEmissiveSetToBrightOrange_ThenValueIsStored) {
        engine::MaterialUniformData data;
        data.emissiveInfo = glm::vec4(1.0f, 0.5f, 0.0f, 10.0f);
        EXPECT_NEAR(data.emissiveInfo.a, 10.0f, EPSILON);
    }
    TEST(MaterialUniformData, GivenData_WhenAttenuationSetToGreenTint_ThenValueIsStored) {
        engine::MaterialUniformData data;
        data.attenuationColorAndDist = glm::vec4(0.2f, 0.8f, 0.2f, 0.5f);
        EXPECT_NEAR(data.attenuationColorAndDist.g, 0.8f, EPSILON);
        EXPECT_NEAR(data.attenuationColorAndDist.a, 0.5f, EPSILON);
    }
    TEST(MaterialUniformData, GivenOriginal_WhenCopyConstructed_ThenValuesMatch) {
        engine::MaterialUniformData original;
        original.albedo                  = glm::vec4(0.5f, 0.3f, 0.1f, 1.0f);
        original.params[0]               = glm::vec4(1.0f, 0.2f, 1.0f, 0.0f);
        engine::MaterialUniformData copy = original;
        EXPECT_EQ(copy.albedo, original.albedo);
        EXPECT_EQ(copy.params[0], original.params[0]);
    }
    TEST(MaterialUniformData, GivenOriginal_WhenCopyAssigned_ThenValuesMatch) {
        engine::MaterialUniformData original;
        original.indices1 = glm::uvec4(1, 2, 3, 4);
        engine::MaterialUniformData copy;
        copy = original;
        EXPECT_EQ(copy.indices1, original.indices1);
    }
    TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenVec4MembersAre16ByteAligned) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, albedo) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, emissiveInfo) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, specularGlossinessFactor) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, attenuationColorAndDist) % 16, 0);
    }
    TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenMat4MemberIs16ByteAligned) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, params) % 16, 0);
    }
    TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenUVec4MembersAre16ByteAligned) {
        EXPECT_EQ(offsetof(engine::MaterialUniformData, flagsAndIndices0) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, indices1) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, indices2) % 16, 0);
        EXPECT_EQ(offsetof(engine::MaterialUniformData, indices3) % 16, 0);
    }
}  // namespace
