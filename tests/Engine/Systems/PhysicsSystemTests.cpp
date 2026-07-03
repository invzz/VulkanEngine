#include <gtest/gtest.h>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {
    class PhysicsSystemTests : public ::testing::Test {
       protected:
        void SetUp() override {
            scene = std::make_unique<Scene>();
        }

        std::unique_ptr<Scene> scene;
    };

    TEST_F(PhysicsSystemTests, GivenRigidBodyComponent_WhenUpdateCalled_ThenVelocityIsUpdated) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(1.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.isStatic        = false;
        rigidBody.useGravity      = false;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_FLOAT_EQ(transform.translation.x, 1.0f);
        EXPECT_FLOAT_EQ(transform.translation.y, 0.0f);
        EXPECT_FLOAT_EQ(transform.translation.z, 0.0f);
    }

    TEST_F(PhysicsSystemTests, GivenStaticRigidBody_WhenUpdateCalled_ThenPositionIsUnchanged) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(10.0f, 20.0f, 30.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(1.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.isStatic        = true;
        rigidBody.useGravity      = false;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_FLOAT_EQ(transform.translation.x, 10.0f);
        EXPECT_FLOAT_EQ(transform.translation.y, 20.0f);
        EXPECT_FLOAT_EQ(transform.translation.z, 30.0f);
    }

    TEST_F(PhysicsSystemTests, GivenRigidBodyWithGravity_WhenUpdateCalled_ThenVelocityIsUpdated) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.isStatic        = false;
        rigidBody.useGravity      = true;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        float expectedVelocityY = -9.81f * rigidBody.mass;
        EXPECT_FLOAT_EQ(rigidBody.velocity.y, expectedVelocityY);
    }

    TEST_F(PhysicsSystemTests, GivenRigidBodyWithAngularVelocity_WhenUpdateCalled_ThenRotationIsUpdated) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 1.0f, 0.0f);
        rigidBody.isStatic        = false;
        rigidBody.useGravity      = false;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_FLOAT_EQ(transform.rotation.x, 0.0f);
        EXPECT_FLOAT_EQ(transform.rotation.y, 1.0f);
        EXPECT_FLOAT_EQ(transform.rotation.z, 0.0f);
    }

    TEST_F(PhysicsSystemTests, GivenRigidBodyWithAcceleration_WhenUpdateCalled_ThenVelocityIsUpdated) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 1.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.isStatic        = false;
        rigidBody.useGravity      = false;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_FLOAT_EQ(rigidBody.velocity.x, 0.0f);
        EXPECT_FLOAT_EQ(rigidBody.velocity.y, 1.0f);
        EXPECT_FLOAT_EQ(rigidBody.velocity.z, 0.0f);
    }

    TEST_F(PhysicsSystemTests, GivenMultipleRigidBodies_WhenUpdateCalled_ThenAllAreProcessed) {
        auto entity1 = scene->getRegistry().create();
        auto entity2 = scene->getRegistry().create();

        auto& transform1       = scene->getRegistry().emplace<TransformComponent>(entity1);
        transform1.translation = glm::vec3(0.0f, 0.0f, 0.0f);
        transform1.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform1.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody1           = scene->getRegistry().emplace<RigidBodyComponent>(entity1);
        rigidBody1.mass            = 1.0f;
        rigidBody1.velocity        = glm::vec3(1.0f, 0.0f, 0.0f);
        rigidBody1.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody1.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody1.isStatic        = false;
        rigidBody1.useGravity      = false;

        auto& transform2       = scene->getRegistry().emplace<TransformComponent>(entity2);
        transform2.translation = glm::vec3(5.0f, 0.0f, 0.0f);
        transform2.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform2.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody2           = scene->getRegistry().emplace<RigidBodyComponent>(entity2);
        rigidBody2.mass            = 1.0f;
        rigidBody2.velocity        = glm::vec3(0.0f, 1.0f, 0.0f);
        rigidBody2.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody2.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody2.isStatic        = false;
        rigidBody2.useGravity      = false;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_FLOAT_EQ(transform1.translation.x, 1.0f);
        EXPECT_FLOAT_EQ(transform1.translation.y, 0.0f);
        EXPECT_FLOAT_EQ(transform1.translation.z, 0.0f);

        EXPECT_FLOAT_EQ(transform2.translation.x, 5.0f);
        EXPECT_FLOAT_EQ(transform2.translation.y, 1.0f);
        EXPECT_FLOAT_EQ(transform2.translation.z, 0.0f);
    }

    TEST_F(PhysicsSystemTests, GivenRigidBodyWithGravity_WhenUpdateCalled_ThenPositionIsAffected) {
        auto entity = scene->getRegistry().create();

        auto& transform       = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, 10.0f, 0.0f);
        transform.rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.scale       = glm::vec3(1.0f, 1.0f, 1.0f);

        auto& rigidBody           = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.mass            = 1.0f;
        rigidBody.velocity        = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.acceleration    = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.isStatic        = false;
        rigidBody.useGravity      = true;

        Camera    camera;
        FrameInfo frameInfo{
            .frameIndex          = 0,
            .frameTime           = 0.016f,
            .commandBuffer       = VK_NULL_HANDLE,
            .camera              = camera,
            .globalDescriptorSet = VK_NULL_HANDLE,
            .globalTextureSet    = VK_NULL_HANDLE,
            .scene               = scene.get(),
            .selectedObjectId    = 0,
            .selectedEntity      = entt::null,
            .cameraEntity        = entt::null,
            .morphManager        = nullptr,
            .extent              = {1920, 1080},
            .debugMode           = 0};

        PhysicsSystem::update(frameInfo);

        EXPECT_LT(transform.translation.y, 10.0f);
    }
}  // namespace engine