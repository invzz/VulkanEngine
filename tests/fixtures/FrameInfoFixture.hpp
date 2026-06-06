/**
 * @file FrameInfoFixture.hpp
 * @brief Shared test fixture for system tests that need FrameInfo
 *
 * Provides common setup for render system tests that require FrameInfo,
 * Camera, and Scene instances.
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_FRAMEINFOFIXTURE_HPP
#define VULKANENGINE_TESTS_FIXTURES_FRAMEINFOFIXTURE_HPP

#include <gtest/gtest.h>
#include <memory>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

#include "DeviceFixture.hpp"

namespace engine::test {

    /**
 * @brief Fixture for render system tests requiring FrameInfo
 *
 * Provides:
 * - Shared Device (from DeviceFixture)
 * - Helper to create valid FrameInfo structures
 * - Common extent settings for testing
 */
    class FrameInfoFixture : public DeviceFixture {
       protected:
        // Default test extent
        static constexpr VkExtent2D kDefaultExtent = {64, 64};

        // Helper to create a valid FrameInfo with the given camera and scene
        static FrameInfo makeFrameInfo(Camera& camera, Scene* scene, VkExtent2D extent = kDefaultExtent) {
            return FrameInfo{
                .frameIndex          = 0,
                .frameTime           = 0.0f,
                .commandBuffer       = VK_NULL_HANDLE,
                .camera              = camera,
                .globalDescriptorSet = VK_NULL_HANDLE,
                .globalTextureSet    = VK_NULL_HANDLE,
                .scene               = scene,
                .selectedObjectId    = 0,
                .selectedEntity      = entt::null,
                .cameraEntity        = entt::null,
                .morphManager        = nullptr,
                .extent              = extent,
                .debugMode           = 0,
            };
        }

        // Helper to create a camera with common test settings
        static Camera createTestCamera(float fov = 45.0f, float near = 0.1f, float far = 100.0f) {
            Camera camera;
            camera.setPerspectiveProjection(glm::radians(fov), 1.0f, near, far);
            return camera;
        }
    };

    /**
 * @brief Fixture with pre-created Camera and Scene
 *
 * Extends FrameInfoFixture with ready-to-use Camera and Scene instances.
 */
    class FrameInfoWithSceneFixture : public FrameInfoFixture {
       protected:
        void SetUp() override {
            camera_ = createTestCamera();
            scene_  = std::make_unique<Scene>();
        }

        void TearDown() override {
            scene_.reset();
        }

        Camera& camera() {
            return camera_;
        }
        Scene& scene() {
            return *scene_;
        }

        // Create FrameInfo for the current camera/scene
        FrameInfo makeFrameInfo(VkExtent2D extent = kDefaultExtent) {
            return FrameInfoFixture::makeFrameInfo(camera_, scene_.get(), extent);
        }

       private:
        Camera                 camera_;
        std::unique_ptr<Scene> scene_;
    };

}  // namespace engine::test

#endif  // VULKANENGINE_TESTS_FIXTURES_FRAMEINFOFIXTURE_HPP
