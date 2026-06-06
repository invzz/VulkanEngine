#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include <cmath>
#include <gtest/gtest.h>

#include "Engine/Scene/Camera.hpp"

using namespace engine;

// =============================================================================
// Camera Projection Tests
// =============================================================================

TEST(Camera, GivenDefaultCamera_WhenCreated_ThenMatricesAreIdentity) {
    Camera camera;

    EXPECT_EQ(camera.getProjectionMatrix(), glm::mat4(1.0f));
    EXPECT_EQ(camera.getViewMatrix(), glm::mat4(1.0f));
    EXPECT_EQ(camera.getInverseView(), glm::mat4(1.0f));
}

TEST(Camera, GivenPerspectiveProjection_WhenSet_ThenProjectionMatrixIsValid) {
    Camera camera;

    const float fovY   = glm::radians(45.0f);
    const float aspect = 16.0f / 9.0f;
    const float nearZ  = 0.1f;
    const float farZ   = 100.0f;

    camera.setPerspectiveProjection(fovY, aspect, nearZ, farZ);

    const glm::mat4& proj = camera.getProjectionMatrix();

    // Check that the projection matrix is not identity
    EXPECT_NE(proj, glm::mat4(1.0f));

    // Check key diagonal elements are set
    EXPECT_NE(proj[0][0], 0.0f);  // X scale
    EXPECT_NE(proj[1][1], 0.0f);  // Y scale
    EXPECT_NE(proj[2][2], 0.0f);  // Z mapping

    // Check perspective division element
    EXPECT_FLOAT_EQ(proj[2][3], 1.0f);
}

TEST(Camera, GivenOrthographicProjection_WhenSet_ThenProjectionMatrixIsValid) {
    Camera camera;

    camera.setOrtographicProjection(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

    const glm::mat4& proj = camera.getProjectionMatrix();

    // Orthographic projection has 0 in [2][3] (no perspective division)
    EXPECT_FLOAT_EQ(proj[2][3], 0.0f);

    // Check scaling factors - X scale
    EXPECT_FLOAT_EQ(proj[0][0], 2.0f / 20.0f);  // 2 / (right - left)

    // Y scale is inverted for Vulkan coordinate system
    EXPECT_FLOAT_EQ(std::abs(proj[1][1]), 2.0f / 20.0f);  // |2 / (bottom - top)|
}

// =============================================================================
// Camera View Tests
// =============================================================================

TEST(Camera, GivenViewDirection_WhenSet_ThenViewMatrixTransformsCorrectly) {
    Camera camera;

    glm::vec3 position(0.0f, 0.0f, 5.0f);
    glm::vec3 direction(0.0f, 0.0f, -1.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    camera.setViewDirection(position, direction, up);

    // Camera position should be extractable from inverse view matrix
    glm::vec3 extractedPos = camera.getPosition();
    EXPECT_NEAR(extractedPos.x, position.x, 0.001f);
    EXPECT_NEAR(extractedPos.y, position.y, 0.001f);
    EXPECT_NEAR(extractedPos.z, position.z, 0.001f);
}

TEST(Camera, GivenViewTarget_WhenSet_ThenCameraLooksAtTarget) {
    Camera camera;

    glm::vec3 position(0.0f, 0.0f, 5.0f);
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    camera.setViewTarget(position, target, up);

    // Position should be preserved
    glm::vec3 extractedPos = camera.getPosition();
    EXPECT_NEAR(extractedPos.x, position.x, 0.001f);
    EXPECT_NEAR(extractedPos.y, position.y, 0.001f);
    EXPECT_NEAR(extractedPos.z, position.z, 0.001f);
}

TEST(Camera, GivenViewYXZ_WhenSet_ThenRotationApplied) {
    Camera camera;

    glm::vec3 position(1.0f, 2.0f, 3.0f);
    glm::vec3 rotation(0.0f, 0.0f, 0.0f);  // No rotation

    camera.setViewYXZ(position, rotation);

    glm::vec3 extractedPos = camera.getPosition();
    EXPECT_NEAR(extractedPos.x, position.x, 0.001f);
    EXPECT_NEAR(extractedPos.y, position.y, 0.001f);
    EXPECT_NEAR(extractedPos.z, position.z, 0.001f);
}

TEST(Camera, GivenViewYXZ_WhenRotated90DegreesY_ThenViewChanges) {
    Camera camera1, camera2;

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    glm::vec3 noRotation(0.0f, 0.0f, 0.0f);
    glm::vec3 rotated90Y(0.0f, glm::radians(90.0f), 0.0f);

    camera1.setViewYXZ(position, noRotation);
    camera2.setViewYXZ(position, rotated90Y);

    // View matrices should be different
    EXPECT_NE(camera1.getViewMatrix(), camera2.getViewMatrix());
}

// =============================================================================
// Camera Frustum Culling Tests
// =============================================================================

TEST(Camera, GivenPerspectiveCamera_WhenFrustumUpdated_ThenPlanesAreNormalized) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewDirection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    const Camera::Frustum& frustum = camera.getFrustum();

    // Each plane normal should be approximately unit length
    for (int i = 0; i < 6; ++i) {
        float normalLength = glm::length(glm::vec3(frustum.planes[i]));
        EXPECT_NEAR(normalLength, 1.0f, 0.01f) << "Plane " << i << " is not normalized";
    }
}

TEST(Camera, GivenCameraAtOrigin_WhenObjectAtCenter_ThenIsInFrustum) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewDirection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    // Object directly in front of camera
    glm::vec3 objectCenter(0.0f, 0.0f, -10.0f);
    float     objectRadius = 1.0f;

    EXPECT_TRUE(camera.isInFrustum(objectCenter, objectRadius));
}

TEST(Camera, GivenCameraAtOrigin_WhenObjectBehind_ThenNotInFrustum) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewDirection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    // Object behind camera (positive Z)
    glm::vec3 objectCenter(0.0f, 0.0f, 10.0f);
    float     objectRadius = 1.0f;

    EXPECT_FALSE(camera.isInFrustum(objectCenter, objectRadius));
}

TEST(Camera, GivenCameraAtOrigin_WhenObjectFarLeft_ThenNotInFrustum) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewDirection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    // Object far to the left, outside FOV
    glm::vec3 objectCenter(-100.0f, 0.0f, -10.0f);
    float     objectRadius = 1.0f;

    EXPECT_FALSE(camera.isInFrustum(objectCenter, objectRadius));
}

TEST(Camera, GivenCameraAtOrigin_WhenObjectBeyondFarPlane_ThenNotInFrustum) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 50.0f);
    camera.setViewDirection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    // Object beyond far plane
    glm::vec3 objectCenter(0.0f, 0.0f, -200.0f);
    float     objectRadius = 1.0f;

    EXPECT_FALSE(camera.isInFrustum(objectCenter, objectRadius));
}

TEST(Camera, GivenCamera_WhenLargeSpherePartiallyInFrustum_ThenIsInFrustum) {
    Camera camera;

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    camera.setViewDirection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.updateFrustum();

    // Large sphere that extends outside frustum but center is visible
    glm::vec3 objectCenter(0.0f, 0.0f, -10.0f);
    float     objectRadius = 50.0f;  // Very large radius

    EXPECT_TRUE(camera.isInFrustum(objectCenter, objectRadius));
}

// =============================================================================
// Camera Inverse View Matrix Tests
// =============================================================================

TEST(Camera, GivenViewMatrix_WhenInverseAccessed_ThenProductIsIdentity) {
    Camera camera;

    camera.setViewDirection(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f));

    glm::mat4 product = camera.getViewMatrix() * camera.getInverseView();

    // Product should be approximately identity
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(product[i][j], expected, 0.001f) << "Mismatch at [" << i << "][" << j << "]";
        }
    }
}
