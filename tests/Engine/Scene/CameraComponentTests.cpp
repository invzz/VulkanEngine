/**
 * @file CameraComponentTests.cpp
 * @brief Unit tests for the CameraComponent struct
 *
 * Tests default values, perspective/orthographic settings, and camera properties.
 */

#include <glm/glm.hpp>

#include <gtest/gtest.h>

#include "Engine/Scene/components/CameraComponent.hpp"

namespace {

    constexpr float EPSILON = 1e-6f;

    // ===========================================================================
    // Default Value Tests
    // ===========================================================================

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenFovYIs80) {
        engine::CameraComponent component;
        EXPECT_NEAR(component.fovY, 80.0f, EPSILON);
    }

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenNearZIsPointOne) {
        engine::CameraComponent component;
        EXPECT_NEAR(component.nearZ, 0.1f, EPSILON);
    }

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenFarZIs100) {
        engine::CameraComponent component;
        EXPECT_NEAR(component.farZ, 100.0f, EPSILON);
    }

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenOrthoSizeIs10) {
        engine::CameraComponent component;
        EXPECT_NEAR(component.orthoSize, 10.0f, EPSILON);
    }

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenIsNotOrthographic) {
        engine::CameraComponent component;
        EXPECT_FALSE(component.isOrthographic);
    }

    TEST(CameraComponent, GivenDefaultConstruction_WhenInspected_ThenIsPrimary) {
        engine::CameraComponent component;
        EXPECT_TRUE(component.isPrimary);
    }

    // ===========================================================================
    // Camera Object Tests
    // ===========================================================================

    TEST(CameraComponent, GivenDefaultConstruction_WhenCameraInspected_ThenMatricesAreIdentity) {
        engine::CameraComponent component;

        // The embedded camera should have identity matrices by default
        EXPECT_EQ(component.camera.getProjectionMatrix(), glm::mat4(1.0f));
        EXPECT_EQ(component.camera.getViewMatrix(), glm::mat4(1.0f));
    }

    TEST(CameraComponent, GivenComponent_WhenPerspectiveProjectionSet_ThenMatrixChanges) {
        engine::CameraComponent component;

        component.camera.setPerspectiveProjection(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

        // Projection should no longer be identity
        EXPECT_NE(component.camera.getProjectionMatrix(), glm::mat4(1.0f));
    }

    TEST(CameraComponent, GivenComponent_WhenViewDirectionSet_ThenMatrixChanges) {
        engine::CameraComponent component;

        component.camera.setViewDirection({0, 0, -5}, {0, 0, 1});

        // View should no longer be identity
        EXPECT_NE(component.camera.getViewMatrix(), glm::mat4(1.0f));
    }

    // ===========================================================================
    // Property Modification Tests
    // ===========================================================================

    TEST(CameraComponent, GivenComponent_WhenFovYSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.fovY = 60.0f;

        EXPECT_NEAR(component.fovY, 60.0f, EPSILON);
    }

    TEST(CameraComponent, GivenComponent_WhenNearZSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.nearZ = 0.01f;

        EXPECT_NEAR(component.nearZ, 0.01f, EPSILON);
    }

    TEST(CameraComponent, GivenComponent_WhenFarZSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.farZ = 10000.0f;

        EXPECT_NEAR(component.farZ, 10000.0f, EPSILON);
    }

    TEST(CameraComponent, GivenComponent_WhenOrthoSizeSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.orthoSize = 50.0f;

        EXPECT_NEAR(component.orthoSize, 50.0f, EPSILON);
    }

    TEST(CameraComponent, GivenComponent_WhenIsOrthographicSetTrue_ThenValueIsStored) {
        engine::CameraComponent component;
        component.isOrthographic = true;

        EXPECT_TRUE(component.isOrthographic);
    }

    TEST(CameraComponent, GivenComponent_WhenIsPrimarySetFalse_ThenValueIsStored) {
        engine::CameraComponent component;
        component.isPrimary = false;

        EXPECT_FALSE(component.isPrimary);
    }

    // ===========================================================================
    // Use Case Tests
    // ===========================================================================

    TEST(CameraComponent, GivenPerspectiveSettings_WhenApplied_ThenCameraIsConfigured) {
        engine::CameraComponent component;
        component.fovY           = 90.0f;
        component.nearZ          = 0.1f;
        component.farZ           = 500.0f;
        component.isOrthographic = false;
        component.isPrimary      = true;

        // Apply settings to the camera
        float aspect = 16.0f / 9.0f;
        component.camera.setPerspectiveProjection(glm::radians(component.fovY), aspect, component.nearZ, component.farZ);

        // Verify projection was set
        EXPECT_NE(component.camera.getProjectionMatrix(), glm::mat4(1.0f));
        EXPECT_FALSE(component.isOrthographic);
    }

    TEST(CameraComponent, GivenOrthographicSettings_WhenApplied_ThenCameraIsConfigured) {
        engine::CameraComponent component;
        component.orthoSize      = 20.0f;
        component.nearZ          = -100.0f;
        component.farZ           = 100.0f;
        component.isOrthographic = true;
        component.isPrimary      = true;

        // Apply orthographic projection
        float halfHeight = component.orthoSize / 2.0f;
        float aspect     = 16.0f / 9.0f;
        float halfWidth  = halfHeight * aspect;

        component.camera.setOrtographicProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, component.nearZ, component.farZ);

        // Verify projection was set
        EXPECT_NE(component.camera.getProjectionMatrix(), glm::mat4(1.0f));
        EXPECT_TRUE(component.isOrthographic);
    }

    TEST(CameraComponent, GivenMultipleCameras_WhenConfigured_ThenPrimaryCanBeDistinguished) {
        engine::CameraComponent mainCamera;
        mainCamera.isPrimary = true;

        engine::CameraComponent secondaryCamera;
        secondaryCamera.isPrimary = false;
        secondaryCamera.fovY      = 45.0f;  // Different FOV for secondary

        EXPECT_TRUE(mainCamera.isPrimary);
        EXPECT_FALSE(secondaryCamera.isPrimary);
        EXPECT_NE(mainCamera.fovY, secondaryCamera.fovY);
    }

    // ===========================================================================
    // Edge Cases
    // ===========================================================================

    TEST(CameraComponent, GivenVerySmallNearPlane_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.nearZ = 0.001f;

        EXPECT_NEAR(component.nearZ, 0.001f, EPSILON);
    }

    TEST(CameraComponent, GivenVeryLargeFarPlane_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.farZ = 100000.0f;

        EXPECT_NEAR(component.farZ, 100000.0f, EPSILON);
    }

    TEST(CameraComponent, GivenNarrowFOV_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.fovY = 10.0f;  // Very zoomed in

        EXPECT_NEAR(component.fovY, 10.0f, EPSILON);
    }

    TEST(CameraComponent, GivenWideFOV_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.fovY = 120.0f;  // Fisheye-like

        EXPECT_NEAR(component.fovY, 120.0f, EPSILON);
    }

    TEST(CameraComponent, GivenSmallOrthoSize_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.orthoSize = 1.0f;

        EXPECT_NEAR(component.orthoSize, 1.0f, EPSILON);
    }

    TEST(CameraComponent, GivenLargeOrthoSize_WhenSet_ThenValueIsStored) {
        engine::CameraComponent component;
        component.orthoSize = 1000.0f;

        EXPECT_NEAR(component.orthoSize, 1000.0f, EPSILON);
    }

    // ===========================================================================
    // Copy and Assignment Tests
    // ===========================================================================

    TEST(CameraComponent, GivenOriginal_WhenCopyConstructed_ThenValuesMatch) {
        engine::CameraComponent original;
        original.fovY           = 45.0f;
        original.nearZ          = 0.5f;
        original.farZ           = 200.0f;
        original.orthoSize      = 25.0f;
        original.isOrthographic = true;
        original.isPrimary      = false;

        engine::CameraComponent copy = original;

        EXPECT_NEAR(copy.fovY, 45.0f, EPSILON);
        EXPECT_NEAR(copy.nearZ, 0.5f, EPSILON);
        EXPECT_NEAR(copy.farZ, 200.0f, EPSILON);
        EXPECT_NEAR(copy.orthoSize, 25.0f, EPSILON);
        EXPECT_TRUE(copy.isOrthographic);
        EXPECT_FALSE(copy.isPrimary);
    }

    TEST(CameraComponent, GivenOriginal_WhenCopyAssigned_ThenValuesMatch) {
        engine::CameraComponent original;
        original.fovY = 30.0f;

        engine::CameraComponent copy;
        copy = original;

        EXPECT_NEAR(copy.fovY, 30.0f, EPSILON);
    }

    // ===========================================================================
    // Combined Camera Operations
    // ===========================================================================

    TEST(CameraComponent, GivenCameraWithFrustum_WhenUpdated_ThenCullingWorks) {
        engine::CameraComponent component;
        component.fovY  = 60.0f;
        component.nearZ = 0.1f;
        component.farZ  = 100.0f;

        // Set up camera for frustum culling
        component.camera.setPerspectiveProjection(glm::radians(component.fovY), 1.0f, component.nearZ, component.farZ);

        component.camera.setViewDirection({0, 0, -10}, {0, 0, 1});
        component.camera.updateFrustum();

        // Object at origin should be in frustum
        EXPECT_TRUE(component.camera.isInFrustum({0, 0, 0}, 1.0f));
    }

    TEST(CameraComponent, GivenCameraWithPosition_WhenExtracted_ThenPositionIsCorrect) {
        engine::CameraComponent component;

        glm::vec3 position{10.0f, 5.0f, -3.0f};
        component.camera.setViewDirection(position, {0, 0, 1});

        glm::vec3 extractedPos = component.camera.getPosition();

        EXPECT_NEAR(extractedPos.x, position.x, EPSILON);
        EXPECT_NEAR(extractedPos.y, position.y, EPSILON);
        EXPECT_NEAR(extractedPos.z, position.z, EPSILON);
    }

}  // namespace
