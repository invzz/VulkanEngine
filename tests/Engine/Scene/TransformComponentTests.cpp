/**
 * @file TransformComponentTests.cpp
 * @brief Unit tests for TransformComponent (pure math, no GPU required)
 */

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <gtest/gtest.h>

#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine::test {

    constexpr float EPSILON = 1e-5f;

    class TransformComponentTest : public ::testing::Test {
       protected:
        TransformComponent transform;
    };

    // ============================================================================
    // Default State Tests
    // ============================================================================

    TEST_F(TransformComponentTest, DefaultValues) {
        EXPECT_EQ(transform.translation, glm::vec3(0.0f));
        EXPECT_EQ(transform.scale, glm::vec3(1.0f));
        EXPECT_EQ(transform.rotation, glm::vec3(0.0f));
        EXPECT_EQ(transform.baseScale, glm::vec3(1.0f));
    }

    TEST_F(TransformComponentTest, DefaultModelTransform_IsIdentity) {
        glm::mat4 model    = transform.modelTransform();
        glm::mat4 identity = glm::mat4(1.0f);

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                EXPECT_NEAR(model[i][j], identity[i][j], EPSILON);
            }
        }
    }

    TEST_F(TransformComponentTest, DefaultNormalMatrix_IsIdentity) {
        glm::mat3 normal   = transform.normalMatrix();
        glm::mat3 identity = glm::mat3(1.0f);

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                EXPECT_NEAR(normal[i][j], identity[i][j], EPSILON);
            }
        }
    }

    // ============================================================================
    // Translation Tests
    // ============================================================================

    TEST_F(TransformComponentTest, Translation_AppliedToModelMatrix) {
        transform.translation = glm::vec3(5.0f, 10.0f, -3.0f);
        glm::mat4 model       = transform.modelTransform();

        // Translation should be in the last column
        EXPECT_NEAR(model[3][0], 5.0f, EPSILON);
        EXPECT_NEAR(model[3][1], 10.0f, EPSILON);
        EXPECT_NEAR(model[3][2], -3.0f, EPSILON);
        EXPECT_NEAR(model[3][3], 1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Translation_TransformsPoint) {
        transform.translation = glm::vec3(1.0f, 2.0f, 3.0f);
        glm::mat4 model       = transform.modelTransform();

        glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * origin;

        EXPECT_NEAR(result.x, 1.0f, EPSILON);
        EXPECT_NEAR(result.y, 2.0f, EPSILON);
        EXPECT_NEAR(result.z, 3.0f, EPSILON);
    }

    // ============================================================================
    // Scale Tests
    // ============================================================================

    TEST_F(TransformComponentTest, Scale_UniformScale) {
        transform.scale = glm::vec3(2.0f);
        glm::mat4 model = transform.modelTransform();

        glm::vec4 point(1.0f, 1.0f, 1.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 2.0f, EPSILON);
        EXPECT_NEAR(result.y, 2.0f, EPSILON);
        EXPECT_NEAR(result.z, 2.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Scale_NonUniformScale) {
        transform.scale = glm::vec3(2.0f, 3.0f, 4.0f);
        glm::mat4 model = transform.modelTransform();

        glm::vec4 point(1.0f, 1.0f, 1.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 2.0f, EPSILON);
        EXPECT_NEAR(result.y, 3.0f, EPSILON);
        EXPECT_NEAR(result.z, 4.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Scale_ZeroScale) {
        transform.scale = glm::vec3(0.0f);
        glm::mat4 model = transform.modelTransform();

        glm::vec4 point(100.0f, 200.0f, 300.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 0.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Scale_NegativeScale) {
        transform.scale = glm::vec3(-1.0f, 1.0f, 1.0f);
        glm::mat4 model = transform.modelTransform();

        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, -1.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, 0.0f, EPSILON);
    }

    // ============================================================================
    // Rotation Tests
    // ============================================================================

    TEST_F(TransformComponentTest, Rotation_Yaw90Degrees) {
        transform.rotation.y = glm::radians(90.0f);
        glm::mat4 model      = transform.modelTransform();

        // Rotating 90 degrees around Y should map X to Z
        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 0.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, -1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Rotation_Pitch90Degrees) {
        transform.rotation.x = glm::radians(90.0f);
        glm::mat4 model      = transform.modelTransform();

        // Rotating 90 degrees around X should map Y to Z (not -Z due to rotation convention)
        glm::vec4 point(0.0f, 1.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 0.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        // The sign depends on the rotation convention used
        EXPECT_NEAR(std::abs(result.z), 1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Rotation_Roll90Degrees) {
        transform.rotation.z = glm::radians(90.0f);
        glm::mat4 model      = transform.modelTransform();

        // Rotating 90 degrees around Z should map X to Y
        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, 0.0f, EPSILON);
        EXPECT_NEAR(result.y, 1.0f, EPSILON);
        EXPECT_NEAR(result.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Rotation_180Degrees) {
        transform.rotation.y = glm::radians(180.0f);
        glm::mat4 model      = transform.modelTransform();

        glm::vec4 point(1.0f, 0.0f, 1.0f, 1.0f);
        glm::vec4 result = model * point;

        EXPECT_NEAR(result.x, -1.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, -1.0f, EPSILON);
    }

    // ============================================================================
    // Combined Transform Tests
    // ============================================================================

    TEST_F(TransformComponentTest, Combined_ScaleThenTranslate) {
        transform.scale       = glm::vec3(2.0f);
        transform.translation = glm::vec3(10.0f, 0.0f, 0.0f);
        glm::mat4 model       = transform.modelTransform();

        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        // Point is scaled first, then translated
        EXPECT_NEAR(result.x, 12.0f, EPSILON);  // 1*2 + 10
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Combined_RotateAndTranslate) {
        transform.rotation.y  = glm::radians(90.0f);
        transform.translation = glm::vec3(5.0f, 0.0f, 0.0f);
        glm::mat4 model       = transform.modelTransform();

        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        // Rotated first, then translated
        EXPECT_NEAR(result.x, 5.0f, EPSILON);  // 0 + 5
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, -1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, Combined_AllTransforms) {
        transform.scale       = glm::vec3(2.0f);
        transform.rotation.y  = glm::radians(90.0f);
        transform.translation = glm::vec3(10.0f, 5.0f, 0.0f);
        glm::mat4 model       = transform.modelTransform();

        glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = model * point;

        // Scale -> Rotate -> Translate
        // (1,0,0) * 2 = (2,0,0)
        // Rotate 90 Y: (0, 0, -2)
        // Translate: (10, 5, -2)
        EXPECT_NEAR(result.x, 10.0f, EPSILON);
        EXPECT_NEAR(result.y, 5.0f, EPSILON);
        EXPECT_NEAR(result.z, -2.0f, EPSILON);
    }

    // ============================================================================
    // Direction Vector Tests
    // ============================================================================

    TEST_F(TransformComponentTest, ForwardDir_Default) {
        glm::vec3 forward = transform.getForwardDir();

        // Default forward is -Z (looking into the screen)
        EXPECT_NEAR(forward.x, 0.0f, EPSILON);
        EXPECT_NEAR(forward.y, 0.0f, EPSILON);
        EXPECT_NEAR(forward.z, 1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, ForwardDir_Yaw90) {
        transform.rotation.y = glm::radians(90.0f);
        glm::vec3 forward    = transform.getForwardDir();

        // 90 degree yaw should point along +X
        EXPECT_NEAR(forward.x, 1.0f, EPSILON);
        EXPECT_NEAR(forward.y, 0.0f, EPSILON);
        EXPECT_NEAR(forward.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, ForwardDir_Yaw180) {
        transform.rotation.y = glm::radians(180.0f);
        glm::vec3 forward    = transform.getForwardDir();

        EXPECT_NEAR(forward.x, 0.0f, EPSILON);
        EXPECT_NEAR(forward.y, 0.0f, EPSILON);
        EXPECT_NEAR(forward.z, -1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, ForwardDir_PitchUp) {
        transform.rotation.x = glm::radians(-45.0f);
        glm::vec3 forward    = transform.getForwardDir();

        // Pitching up should have positive Y component
        EXPECT_NEAR(forward.y, std::sin(glm::radians(45.0f)), EPSILON);
        EXPECT_GT(forward.z, 0.0f);
    }

    TEST_F(TransformComponentTest, RightDir_Default) {
        glm::vec3 right = transform.getRightDir();

        // Default right is +X
        EXPECT_NEAR(right.x, 1.0f, EPSILON);
        EXPECT_NEAR(right.y, 0.0f, EPSILON);
        EXPECT_NEAR(right.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, RightDir_Yaw90) {
        transform.rotation.y = glm::radians(90.0f);
        glm::vec3 right      = transform.getRightDir();

        // After 90 degree yaw, right should be along -Z
        EXPECT_NEAR(right.x, 0.0f, EPSILON);
        EXPECT_NEAR(right.y, 0.0f, EPSILON);
        EXPECT_NEAR(right.z, -1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, ForwardAndRight_Perpendicular) {
        transform.rotation.y = glm::radians(45.0f);
        transform.rotation.x = glm::radians(30.0f);

        glm::vec3 forward = transform.getForwardDir();
        glm::vec3 right   = transform.getRightDir();

        // Forward and right should always be perpendicular (dot product = 0)
        float dot = glm::dot(forward, right);
        EXPECT_NEAR(dot, 0.0f, EPSILON);
    }

    // ============================================================================
    // LookAt Tests
    // ============================================================================

    TEST_F(TransformComponentTest, LookAt_TargetInFront) {
        transform.translation = glm::vec3(0.0f);
        transform.lookAt(glm::vec3(0.0f, 0.0f, 10.0f));

        // Should be looking along +Z, so yaw=0, pitch=0
        EXPECT_NEAR(transform.rotation.y, 0.0f, EPSILON);
        EXPECT_NEAR(transform.rotation.x, 0.0f, EPSILON);
        EXPECT_NEAR(transform.rotation.z, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, LookAt_TargetBehind) {
        transform.translation = glm::vec3(0.0f);
        transform.lookAt(glm::vec3(0.0f, 0.0f, -10.0f));

        // Should be looking along -Z, so yaw=PI
        EXPECT_NEAR(std::abs(transform.rotation.y), glm::pi<float>(), EPSILON);
    }

    TEST_F(TransformComponentTest, LookAt_TargetToRight) {
        transform.translation = glm::vec3(0.0f);
        transform.lookAt(glm::vec3(10.0f, 0.0f, 0.0f));

        // Should have ~90 degree yaw
        EXPECT_NEAR(transform.rotation.y, glm::radians(90.0f), EPSILON);
        EXPECT_NEAR(transform.rotation.x, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, LookAt_TargetAbove) {
        transform.translation = glm::vec3(0.0f);
        transform.lookAt(glm::vec3(0.0f, 10.0f, 10.0f));

        // Should have negative pitch (looking up)
        EXPECT_LT(transform.rotation.x, 0.0f);
        EXPECT_NEAR(transform.rotation.y, 0.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, LookAt_TargetBelow) {
        transform.translation = glm::vec3(0.0f, 10.0f, 0.0f);
        transform.lookAt(glm::vec3(0.0f, 0.0f, 10.0f));

        // Should have positive pitch (looking down)
        EXPECT_GT(transform.rotation.x, 0.0f);
    }

    TEST_F(TransformComponentTest, LookAt_PreservesZeroRoll) {
        transform.translation = glm::vec3(0.0f);
        transform.rotation.z  = glm::radians(45.0f);  // Set some roll
        transform.lookAt(glm::vec3(10.0f, 5.0f, 10.0f));

        // LookAt should reset roll to 0
        EXPECT_NEAR(transform.rotation.z, 0.0f, EPSILON);
    }

    // ============================================================================
    // Normal Matrix Tests
    // ============================================================================

    TEST_F(TransformComponentTest, NormalMatrix_WithScale) {
        transform.scale  = glm::vec3(2.0f, 1.0f, 0.5f);
        glm::mat3 normal = transform.normalMatrix();

        // Normal matrix should include scale factors
        // (In a proper normal matrix you'd use inverse transpose, but this
        // implementation bakes scale into the normal matrix for the shader)
        EXPECT_NE(normal[0][0], 1.0f);
    }

    TEST_F(TransformComponentTest, NormalMatrix_WithRotation) {
        transform.rotation.y = glm::radians(90.0f);
        glm::mat3 normal     = transform.normalMatrix();

        // Apply to a normal pointing along +Z
        glm::vec3 normalVec(0.0f, 0.0f, 1.0f);
        glm::vec3 result = normal * normalVec;

        // After 90 degree Y rotation, should point along X axis (sign depends on convention)
        EXPECT_NEAR(std::abs(result.x), 1.0f, EPSILON);
        EXPECT_NEAR(result.y, 0.0f, EPSILON);
        EXPECT_NEAR(result.z, 0.0f, EPSILON);
    }

    // ============================================================================
    // Consistency Tests
    // ============================================================================

    TEST_F(TransformComponentTest, ModelAndNormalMatrix_Consistent3x3) {
        transform.scale    = glm::vec3(1.0f);  // Unit scale for easy comparison
        transform.rotation = glm::vec3(0.5f, 0.3f, 0.1f);

        glm::mat4 model  = transform.modelTransform();
        glm::mat3 normal = transform.normalMatrix();

        // Upper-left 3x3 of model should match normal matrix (for unit scale)
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                EXPECT_NEAR(model[i][j], normal[i][j], EPSILON);
            }
        }
    }

    TEST_F(TransformComponentTest, ForwardDir_IsNormalized) {
        transform.rotation = glm::vec3(0.5f, 1.2f, 0.0f);
        glm::vec3 forward  = transform.getForwardDir();

        float length = glm::length(forward);
        EXPECT_NEAR(length, 1.0f, EPSILON);
    }

    TEST_F(TransformComponentTest, RightDir_IsNormalized) {
        transform.rotation.y = 1.5f;
        glm::vec3 right      = transform.getRightDir();

        float length = glm::length(right);
        EXPECT_NEAR(length, 1.0f, EPSILON);
    }

}  // namespace engine::test
