/**
 * @file MaterialUniformDataTests.cpp
 * @brief Unit tests for the MaterialUniformData struct
 *
 * Tests default values and packed parameter layout for GPU uniforms.
 */

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "ModelLib/Resources/MaterialUniformData.hpp"

namespace {

  constexpr float EPSILON = 1e-6f;

  // ===========================================================================
  // Default Value Tests
  // ===========================================================================

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenAlbedoIsWhite)
  {
    engine::MaterialUniformData data;

    EXPECT_NEAR(data.albedo.r, 1.0f, EPSILON);
    EXPECT_NEAR(data.albedo.g, 1.0f, EPSILON);
    EXPECT_NEAR(data.albedo.b, 1.0f, EPSILON);
    EXPECT_NEAR(data.albedo.a, 1.0f, EPSILON);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenEmissiveInfoIsDefault)
  {
    engine::MaterialUniformData data;

    // rgb: color (black), a: strength (1)
    EXPECT_NEAR(data.emissiveInfo.r, 0.0f, EPSILON);
    EXPECT_NEAR(data.emissiveInfo.g, 0.0f, EPSILON);
    EXPECT_NEAR(data.emissiveInfo.b, 0.0f, EPSILON);
    EXPECT_NEAR(data.emissiveInfo.a, 1.0f, EPSILON);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenSpecularGlossinessIsDefault)
  {
    engine::MaterialUniformData data;

    EXPECT_NEAR(data.specularGlossinessFactor.r, 1.0f, EPSILON);
    EXPECT_NEAR(data.specularGlossinessFactor.g, 1.0f, EPSILON);
    EXPECT_NEAR(data.specularGlossinessFactor.b, 1.0f, EPSILON);
    EXPECT_NEAR(data.specularGlossinessFactor.a, 1.0f, EPSILON);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenAttenuationIsDefault)
  {
    engine::MaterialUniformData data;

    // rgb: color (white), a: distance (1)
    EXPECT_NEAR(data.attenuationColorAndDist.r, 1.0f, EPSILON);
    EXPECT_NEAR(data.attenuationColorAndDist.g, 1.0f, EPSILON);
    EXPECT_NEAR(data.attenuationColorAndDist.b, 1.0f, EPSILON);
    EXPECT_NEAR(data.attenuationColorAndDist.a, 1.0f, EPSILON);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenParamsMatrixIsZero)
  {
    engine::MaterialUniformData data;

    for (int col = 0; col < 4; ++col)
    {
      for (int row = 0; row < 4; ++row)
      {
        EXPECT_NEAR(data.params[col][row], 0.0f, EPSILON);
      }
    }
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenFlagsAndIndicesAreZero)
  {
    engine::MaterialUniformData data;

    EXPECT_EQ(data.flagsAndIndices0.x, 0u);
    EXPECT_EQ(data.flagsAndIndices0.y, 0u);
    EXPECT_EQ(data.flagsAndIndices0.z, 0u);
    EXPECT_EQ(data.flagsAndIndices0.w, 0u);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices1AreZero)
  {
    engine::MaterialUniformData data;

    EXPECT_EQ(data.indices1.x, 0u);
    EXPECT_EQ(data.indices1.y, 0u);
    EXPECT_EQ(data.indices1.z, 0u);
    EXPECT_EQ(data.indices1.w, 0u);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices2AreZero)
  {
    engine::MaterialUniformData data;

    EXPECT_EQ(data.indices2.x, 0u);
    EXPECT_EQ(data.indices2.y, 0u);
    EXPECT_EQ(data.indices2.z, 0u);
    EXPECT_EQ(data.indices2.w, 0u);
  }

  TEST(MaterialUniformData, GivenDefaultConstruction_WhenInspected_ThenIndices3AreZero)
  {
    engine::MaterialUniformData data;

    EXPECT_EQ(data.indices3.x, 0u);
    EXPECT_EQ(data.indices3.y, 0u);
    EXPECT_EQ(data.indices3.z, 0u);
    EXPECT_EQ(data.indices3.w, 0u);
  }

  // ===========================================================================
  // Memory Layout Tests (important for GPU alignment)
  // ===========================================================================

  TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenAlbedoIsAtOffset0)
  {
    // albedo should be at offset 0
    EXPECT_EQ(offsetof(engine::MaterialUniformData, albedo), 0);
  }

  TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenParamsIsAfterVec4s)
  {
    // params (mat4) comes after the first 4 vec4s
    // albedo (16) + emissiveInfo (16) + specularGlossinessFactor (16) + attenuationColorAndDist (16) = 64
    EXPECT_EQ(offsetof(engine::MaterialUniformData, params), 64);
  }

  TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenFlagsIsAfterParams)
  {
    // flagsAndIndices0 comes after params (mat4 = 64 bytes)
    // 64 (vec4s) + 64 (mat4) = 128
    EXPECT_EQ(offsetof(engine::MaterialUniformData, flagsAndIndices0), 128);
  }

  TEST(MaterialUniformData, GivenStruct_WhenLayoutChecked_ThenIndicesAreSequential)
  {
    // All uvec4s should be sequential
    size_t flags0Offset   = offsetof(engine::MaterialUniformData, flagsAndIndices0);
    size_t indices1Offset = offsetof(engine::MaterialUniformData, indices1);
    size_t indices2Offset = offsetof(engine::MaterialUniformData, indices2);
    size_t indices3Offset = offsetof(engine::MaterialUniformData, indices3);

    EXPECT_EQ(indices1Offset - flags0Offset, sizeof(glm::uvec4));
    EXPECT_EQ(indices2Offset - indices1Offset, sizeof(glm::uvec4));
    EXPECT_EQ(indices3Offset - indices2Offset, sizeof(glm::uvec4));
  }

  TEST(MaterialUniformData, GivenStruct_WhenSizeChecked_ThenTotalSizeIs192Bytes)
  {
    // Expected total size:
    // 4 * vec4 (albedo, emissiveInfo, specularGlossinessFactor, attenuationColorAndDist) = 64 bytes
    // 1 * mat4 (params) = 64 bytes
    // 4 * uvec4 (flagsAndIndices0, indices1, indices2, indices3) = 64 bytes
    // Total = 192 bytes
    EXPECT_EQ(sizeof(engine::MaterialUniformData), 192);
  }

  // ===========================================================================
  // Packed Parameters Tests (params mat4)
  // ===========================================================================

  TEST(MaterialUniformData, GivenParams_WhenColumn0Set_ThenLayoutMatchesExpected)
  {
    // Col 0: metallic, roughness, ao, isSelected
    engine::MaterialUniformData data;
    data.params[0] = glm::vec4(0.8f, 0.3f, 1.0f, 0.0f); // metallic=0.8, roughness=0.3, ao=1.0, isSelected=0

    EXPECT_NEAR(data.params[0][0], 0.8f, EPSILON); // metallic
    EXPECT_NEAR(data.params[0][1], 0.3f, EPSILON); // roughness
    EXPECT_NEAR(data.params[0][2], 1.0f, EPSILON); // ao
    EXPECT_NEAR(data.params[0][3], 0.0f, EPSILON); // isSelected
  }

  TEST(MaterialUniformData, GivenParams_WhenColumn1Set_ThenLayoutMatchesExpected)
  {
    // Col 1: clearcoat, clearcoatRoughness, anisotropic, anisotropicRotation
    engine::MaterialUniformData data;
    data.params[1] = glm::vec4(1.0f, 0.03f, 0.5f, 0.25f);

    EXPECT_NEAR(data.params[1][0], 1.0f, EPSILON);  // clearcoat
    EXPECT_NEAR(data.params[1][1], 0.03f, EPSILON); // clearcoatRoughness
    EXPECT_NEAR(data.params[1][2], 0.5f, EPSILON);  // anisotropic
    EXPECT_NEAR(data.params[1][3], 0.25f, EPSILON); // anisotropicRotation
  }

  TEST(MaterialUniformData, GivenParams_WhenColumn2Set_ThenLayoutMatchesExpected)
  {
    // Col 2: transmission, ior, iridescence, iridescenceIOR
    engine::MaterialUniformData data;
    data.params[2] = glm::vec4(0.9f, 1.5f, 0.5f, 1.3f);

    EXPECT_NEAR(data.params[2][0], 0.9f, EPSILON); // transmission
    EXPECT_NEAR(data.params[2][1], 1.5f, EPSILON); // ior
    EXPECT_NEAR(data.params[2][2], 0.5f, EPSILON); // iridescence
    EXPECT_NEAR(data.params[2][3], 1.3f, EPSILON); // iridescenceIOR
  }

  TEST(MaterialUniformData, GivenParams_WhenColumn3Set_ThenLayoutMatchesExpected)
  {
    // Col 3: iridescenceThickness, uvScale, alphaCutoff, thickness
    engine::MaterialUniformData data;
    data.params[3] = glm::vec4(100.0f, 2.0f, 0.5f, 0.1f);

    EXPECT_NEAR(data.params[3][0], 100.0f, EPSILON); // iridescenceThickness
    EXPECT_NEAR(data.params[3][1], 2.0f, EPSILON);   // uvScale
    EXPECT_NEAR(data.params[3][2], 0.5f, EPSILON);   // alphaCutoff
    EXPECT_NEAR(data.params[3][3], 0.1f, EPSILON);   // thickness
  }

  // ===========================================================================
  // Packed Flags and Indices Tests
  // ===========================================================================

  TEST(MaterialUniformData, GivenFlagsAndIndices0_WhenSet_ThenLayoutMatchesExpected)
  {
    // x: textureFlags, y: alphaMode, z: albedoIndex, w: normalIndex
    engine::MaterialUniformData data;
    data.flagsAndIndices0 = glm::uvec4(0xFF, 1, 5, 6);

    EXPECT_EQ(data.flagsAndIndices0.x, 0xFF); // textureFlags
    EXPECT_EQ(data.flagsAndIndices0.y, 1u);   // alphaMode
    EXPECT_EQ(data.flagsAndIndices0.z, 5u);   // albedoIndex
    EXPECT_EQ(data.flagsAndIndices0.w, 6u);   // normalIndex
  }

  TEST(MaterialUniformData, GivenIndices1_WhenSet_ThenLayoutMatchesExpected)
  {
    // x: metallicIndex, y: roughnessIndex, z: aoIndex, w: emissiveIndex
    engine::MaterialUniformData data;
    data.indices1 = glm::uvec4(7, 8, 9, 10);

    EXPECT_EQ(data.indices1.x, 7u);  // metallicIndex
    EXPECT_EQ(data.indices1.y, 8u);  // roughnessIndex
    EXPECT_EQ(data.indices1.z, 9u);  // aoIndex
    EXPECT_EQ(data.indices1.w, 10u); // emissiveIndex
  }

  TEST(MaterialUniformData, GivenIndices2_WhenSet_ThenLayoutMatchesExpected)
  {
    // x: specularGlossinessIndex, y: useSpecularGlossiness, z: transmissionIndex, w: clearcoatIndex
    engine::MaterialUniformData data;
    data.indices2 = glm::uvec4(11, 1, 12, 13);

    EXPECT_EQ(data.indices2.x, 11u); // specularGlossinessIndex
    EXPECT_EQ(data.indices2.y, 1u);  // useSpecularGlossiness (bool as uint)
    EXPECT_EQ(data.indices2.z, 12u); // transmissionIndex
    EXPECT_EQ(data.indices2.w, 13u); // clearcoatIndex
  }

  TEST(MaterialUniformData, GivenIndices3_WhenSet_ThenLayoutMatchesExpected)
  {
    // x: clearcoatRoughnessIndex, y: clearcoatNormalIndex, z: pad, w: pad
    engine::MaterialUniformData data;
    data.indices3 = glm::uvec4(14, 15, 0, 0);

    EXPECT_EQ(data.indices3.x, 14u); // clearcoatRoughnessIndex
    EXPECT_EQ(data.indices3.y, 15u); // clearcoatNormalIndex
    EXPECT_EQ(data.indices3.z, 0u);  // pad
    EXPECT_EQ(data.indices3.w, 0u);  // pad
  }

  // ===========================================================================
  // Value Modification Tests
  // ===========================================================================

  TEST(MaterialUniformData, GivenData_WhenAlbedoSetToRed_ThenValueIsStored)
  {
    engine::MaterialUniformData data;
    data.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_NEAR(data.albedo.r, 1.0f, EPSILON);
    EXPECT_NEAR(data.albedo.g, 0.0f, EPSILON);
    EXPECT_NEAR(data.albedo.b, 0.0f, EPSILON);
  }

  TEST(MaterialUniformData, GivenData_WhenEmissiveSetToBrightOrange_ThenValueIsStored)
  {
    engine::MaterialUniformData data;
    data.emissiveInfo = glm::vec4(1.0f, 0.5f, 0.0f, 10.0f); // Orange, strength 10

    EXPECT_NEAR(data.emissiveInfo.a, 10.0f, EPSILON); // Emissive strength
  }

  TEST(MaterialUniformData, GivenData_WhenAttenuationSetToGreenTint_ThenValueIsStored)
  {
    engine::MaterialUniformData data;
    data.attenuationColorAndDist = glm::vec4(0.2f, 0.8f, 0.2f, 0.5f);

    EXPECT_NEAR(data.attenuationColorAndDist.g, 0.8f, EPSILON); // Green dominant
    EXPECT_NEAR(data.attenuationColorAndDist.a, 0.5f, EPSILON); // Distance
  }

  // ===========================================================================
  // Copy and Assignment Tests
  // ===========================================================================

  TEST(MaterialUniformData, GivenOriginal_WhenCopyConstructed_ThenValuesMatch)
  {
    engine::MaterialUniformData original;
    original.albedo    = glm::vec4(0.5f, 0.3f, 0.1f, 1.0f);
    original.params[0] = glm::vec4(1.0f, 0.2f, 1.0f, 0.0f);

    engine::MaterialUniformData copy = original;

    EXPECT_EQ(copy.albedo, original.albedo);
    EXPECT_EQ(copy.params[0], original.params[0]);
  }

  TEST(MaterialUniformData, GivenOriginal_WhenCopyAssigned_ThenValuesMatch)
  {
    engine::MaterialUniformData original;
    original.indices1 = glm::uvec4(1, 2, 3, 4);

    engine::MaterialUniformData copy;
    copy = original;

    EXPECT_EQ(copy.indices1, original.indices1);
  }

  // ===========================================================================
  // GPU Alignment Verification
  // ===========================================================================

  TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenVec4MembersAre16ByteAligned)
  {
    // Vec4 members should be 16-byte aligned
    EXPECT_EQ(offsetof(engine::MaterialUniformData, albedo) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, emissiveInfo) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, specularGlossinessFactor) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, attenuationColorAndDist) % 16, 0);
  }

  TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenMat4MemberIs16ByteAligned)
  {
    // Mat4 should be 16-byte aligned
    EXPECT_EQ(offsetof(engine::MaterialUniformData, params) % 16, 0);
  }

  TEST(MaterialUniformData, GivenStruct_WhenAlignmentChecked_ThenUVec4MembersAre16ByteAligned)
  {
    // UVec4 should be 16-byte aligned
    EXPECT_EQ(offsetof(engine::MaterialUniformData, flagsAndIndices0) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, indices1) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, indices2) % 16, 0);
    EXPECT_EQ(offsetof(engine::MaterialUniformData, indices3) % 16, 0);
  }

} // namespace
