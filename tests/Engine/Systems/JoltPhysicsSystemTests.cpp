#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

namespace engine {
    namespace {
    class JoltPhysicsSystemTests : public ::testing::Test {
    public:
        void SetUp() override {
            scene = std::make_unique<Scene>();
        }

        std::unique_ptr<Scene> scene;
    };
    }  // namespace

    namespace {
        void stepPhysics(JoltPhysicsSystem& physicsSystem, Scene& scene, int steps = 1, float dt = 1.0f / 60.0f) {
            for (int i = 0; i < steps; ++i) {
                physicsSystem.syncToEntities(&scene);
                physicsSystem.update(dt);
                physicsSystem.syncToEntities(&scene);
            }
        }
    }  // namespace

    TEST_F(JoltPhysicsSystemTests, GivenRigidBodyWithoutCollider_WhenSimulated_ThenItStillFalls) {
        auto entity = scene->getRegistry().create();

        auto& transform = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, -5.0f, 0.0f);
        transform.rotation = glm::vec3(0.0f);
        transform.scale = glm::vec3(1.0f);

        auto& rigidBody = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.useGravity = true;
        rigidBody.isStatic = false;
        rigidBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        JoltPhysicsSystem physicsSystem;
        stepPhysics(physicsSystem, *scene, 4);

        EXPECT_GT(transform.translation.y, -5.0f);
    }

    TEST_F(JoltPhysicsSystemTests, GivenRigidBodyWithCollider_WhenSimulated_ThenItFallsDownward) {
        auto entity = scene->getRegistry().create();

        auto& transform = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, -5.0f, 0.0f);
        transform.rotation = glm::vec3(0.0f);
        transform.scale = glm::vec3(1.0f);

        auto& rigidBody = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.useGravity = true;
        rigidBody.isStatic = false;
        rigidBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        auto& collider = scene->getRegistry().emplace<ColliderComponent>(entity);
        collider.shape = ColliderComponent::ShapeType::Box;
        collider.size = glm::vec3(1.0f);

        JoltPhysicsSystem physicsSystem;
        stepPhysics(physicsSystem, *scene, 4);

        EXPECT_GT(transform.translation.y, -5.0f);
    }

    TEST_F(JoltPhysicsSystemTests, GivenRigidBodyAboveGround_WhenSimulatedForLongEnough_ThenItDoesNotTravelUpward) {
        auto entity = scene->getRegistry().create();

        auto& transform = scene->getRegistry().emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f, -3.0f, 0.0f);
        transform.rotation = glm::vec3(0.0f);
        transform.scale = glm::vec3(1.0f);

        auto& rigidBody = scene->getRegistry().emplace<RigidBodyComponent>(entity);
        rigidBody.useGravity = true;
        rigidBody.isStatic = false;
        rigidBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        auto& collider = scene->getRegistry().emplace<ColliderComponent>(entity);
        collider.shape = ColliderComponent::ShapeType::Box;
        collider.size = glm::vec3(1.0f);

        JoltPhysicsSystem physicsSystem;
        stepPhysics(physicsSystem, *scene, 120);

        EXPECT_GT(transform.translation.y, -3.0f);
        EXPECT_GT( 0.0f, transform.translation.y);
        EXPECT_TRUE(std::isfinite(transform.translation.y));
    }

    TEST_F(JoltPhysicsSystemTests, GivenTopBodyWithGravity_WhenFallingOnGravityOffBody_ThenBodiesDoNotGhostThrough) {
        auto bottom = scene->getRegistry().create();
        auto& bottomTransform = scene->getRegistry().emplace<TransformComponent>(bottom);
        bottomTransform.translation = glm::vec3(0.0f, -20.0f, 0.0f);
        bottomTransform.rotation = glm::vec3(0.0f);
        bottomTransform.scale = glm::vec3(1.0f);

        auto& bottomBody = scene->getRegistry().emplace<RigidBodyComponent>(bottom);
        bottomBody.useGravity = false;
        bottomBody.isStatic = false;
        bottomBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        auto& bottomCollider = scene->getRegistry().emplace<ColliderComponent>(bottom);
        bottomCollider.shape = ColliderComponent::ShapeType::Box;
        bottomCollider.size = glm::vec3(1.0f);

        auto top = scene->getRegistry().create();
        auto& topTransform = scene->getRegistry().emplace<TransformComponent>(top);
        topTransform.translation = glm::vec3(0.0f, -23.0f, 0.0f);
        topTransform.rotation = glm::vec3(0.0f);
        topTransform.scale = glm::vec3(1.0f);

        auto& topBody = scene->getRegistry().emplace<RigidBodyComponent>(top);
        topBody.useGravity = true;
        topBody.isStatic = false;
        topBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        auto& topCollider = scene->getRegistry().emplace<ColliderComponent>(top);
        topCollider.shape = ColliderComponent::ShapeType::Box;
        topCollider.size = glm::vec3(1.0f);

        JoltPhysicsSystem physicsSystem;
        stepPhysics(physicsSystem, *scene, 60);

        // If collision response works, the top center should not move far below the bottom center.
        // With unit boxes, center separation should be around 1.0 at contact.
        EXPECT_LT(topTransform.translation.y - bottomTransform.translation.y, 3.0f);
    }

    TEST_F(JoltPhysicsSystemTests, GivenTopBodyWithGravity_WhenFallingOnStaticBody_ThenBodiesDoNotGhostThrough) {
        auto bottom = scene->getRegistry().create();
        auto& bottomTransform = scene->getRegistry().emplace<TransformComponent>(bottom);
        bottomTransform.translation = glm::vec3(0.0f, -20.0f, 0.0f);
        bottomTransform.rotation = glm::vec3(0.0f);
        bottomTransform.scale = glm::vec3(1.0f);

        auto& bottomBody = scene->getRegistry().emplace<RigidBodyComponent>(bottom);
        bottomBody.useGravity = false;
        bottomBody.isStatic = true;
        bottomBody.mode = RigidBodyComponent::PhysicsMode::Static;

        auto& bottomCollider = scene->getRegistry().emplace<ColliderComponent>(bottom);
        bottomCollider.shape = ColliderComponent::ShapeType::Box;
        bottomCollider.size = glm::vec3(1.0f);
        bottomCollider.isTrigger = false;

        auto top = scene->getRegistry().create();
        auto& topTransform = scene->getRegistry().emplace<TransformComponent>(top);
        topTransform.translation = glm::vec3(0.0f, -23.0f, 0.0f);
        topTransform.rotation = glm::vec3(0.0f);
        topTransform.scale = glm::vec3(1.0f);

        auto& topBody = scene->getRegistry().emplace<RigidBodyComponent>(top);
        topBody.useGravity = true;
        topBody.isStatic = false;
        topBody.mode = RigidBodyComponent::PhysicsMode::Dynamic;

        auto& topCollider = scene->getRegistry().emplace<ColliderComponent>(top);
        topCollider.shape = ColliderComponent::ShapeType::Box;
        topCollider.size = glm::vec3(1.0f);
        topCollider.isTrigger = false;

        JoltPhysicsSystem physicsSystem;
        stepPhysics(physicsSystem, *scene, 60);

        EXPECT_LT(topTransform.translation.y - bottomTransform.translation.y, 3.0f);
        EXPECT_LT(bottomTransform.translation.y + 20.0f, 0.01f);
        EXPECT_GT(bottomTransform.translation.y + 20.0f, -0.01f);
    }

}  // namespace engine
