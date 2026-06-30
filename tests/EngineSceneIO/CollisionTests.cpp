#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "../fixtures/SceneFixture.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"

using namespace engine;

class CollisionTest : public engine::test::SceneFixture {};

// =============================================================================
// Physics Collision Tests
// =============================================================================

TEST_F(CollisionTest, GivenShouldBounceScene_WhenSimulated_ThenDynamicCubeBouncesOffStaticBody) {
    std::string const path = "assets/scenes/test/should_bounce_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity staticEntity  = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
        if (rb.isStatic) {
            staticEntity = entity;
        } else {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(staticEntity != entt::null) << "No static body found in should_bounce_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in should_bounce_scene.json";

    auto& staticTransform  = scene.getRegistry().get<TransformComponent>(staticEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& staticBody       = scene.getRegistry().get<RigidBodyComponent>(staticEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    // Position static floor above dynamic cube so the cube rises and hits it.
    staticTransform.translation.y        = 40.0f;
    dynamicTransform.translation.y       = 35.0f;
    staticBody.pendingBodyStateOverride  = true;
    dynamicBody.pendingBodyStateOverride = true;

    // Force elastic, frictionless contact so the bounce signal is deterministic.
    staticBody.restitution               = 0.95f;
    dynamicBody.restitution              = 0.95f;
    staticBody.friction                  = 0.0f;
    dynamicBody.friction                 = 0.0f;
    dynamicBody.useGravity               = false;
    dynamicBody.velocity                 = glm::vec3(0.0f, 10.0f, 0.0f);  // moving toward static body
    dynamicBody.pendingBodyStateOverride = true;

    float const startY = dynamicTransform.translation.y;

    bool  sawUpwardMotion        = false;
    bool  sawContact             = false;
    bool  sawReboundAfterContact = false;
    float previousY              = startY;

    JoltPhysicsSystem physicsSystem;

    for (int i = 0; i < 120; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f, 8, 1.0f / 240.0f);
        physicsSystem.syncToEntities(&scene);

        float const separationY    = dynamicTransform.translation.y - staticTransform.translation.y;
        float const absSeparationY = std::abs(separationY);

        if (dynamicBody.velocity.y > 0.2f) {
            sawUpwardMotion = true;
        }
        if (absSeparationY < 1.25f) {
            sawContact = true;
        }
        if (sawContact && dynamicBody.velocity.y < -0.05f) {
            sawReboundAfterContact = true;
            break;
        }
        if (sawContact && dynamicTransform.translation.y < previousY - 0.001f) {
            sawReboundAfterContact = true;
            break;
        }

        previousY = dynamicTransform.translation.y;
    }

    // The cube should have moved toward the static body.
    EXPECT_GT(dynamicTransform.translation.y, startY);
    // Full bounce signature: upward motion, contact, then rebound.
    EXPECT_TRUE(sawUpwardMotion) << "Dynamic body never moved upward";
    EXPECT_TRUE(sawContact) << "Bodies never came into contact";
    EXPECT_TRUE(sawReboundAfterContact) << "Dynamic body did not rebound after contact";
}

TEST_F(CollisionTest, GivenShouldBounceSphereScene_WhenSimulated_ThenDynamicSphereBouncesOffStaticBox) {
    std::string const path = "assets/scenes/test/should_bounce_sphere_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity staticEntity  = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
        if (rb.isStatic) {
            staticEntity = entity;
        } else {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(staticEntity != entt::null) << "No static body found in should_bounce_sphere_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in should_bounce_sphere_scene.json";

    auto& staticTransform  = scene.getRegistry().get<TransformComponent>(staticEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    float const startY = dynamicTransform.translation.y;

    bool  sawForwardMotion = false;
    bool  sawContact       = false;
    bool  sawRebound       = false;
    float previousY        = startY;

    JoltPhysicsSystem physicsSystem;

    for (int i = 0; i < 120; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f);
        physicsSystem.syncToEntities(&scene);

        float const separationY    = dynamicTransform.translation.y - staticTransform.translation.y;
        float const absSeparationY = std::abs(separationY);

        if (dynamicBody.velocity.y > 0.2f) {
            sawForwardMotion = true;
        }
        if (absSeparationY < 1.25f) {
            sawContact = true;
        }
        if (sawContact && dynamicBody.velocity.y < -0.05f) {
            sawRebound = true;
            break;
        }
        if (sawContact && dynamicTransform.translation.y < previousY - 0.001f) {
            sawRebound = true;
            break;
        }

        previousY = dynamicTransform.translation.y;
    }

    EXPECT_GT(dynamicTransform.translation.y, startY);
    EXPECT_TRUE(sawForwardMotion) << "Dynamic sphere never moved toward the static body";
    EXPECT_TRUE(sawContact) << "Sphere and box never came into contact";
    EXPECT_TRUE(sawRebound) << "Dynamic sphere did not rebound after contact";
}

TEST_F(CollisionTest, GivenShouldMissStaticScene_WhenSimulated_ThenBodiesDoNotCollide) {
    std::string const path = "assets/scenes/test/should_miss_static_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity staticEntity  = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
        if (rb.isStatic) {
            staticEntity = entity;
        } else {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(staticEntity != entt::null) << "No static body found in should_miss_static_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in should_miss_static_scene.json";

    auto& staticTransform  = scene.getRegistry().get<TransformComponent>(staticEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    float const startY      = dynamicTransform.translation.y;
    float       minDistance = std::numeric_limits<float>::max();

    JoltPhysicsSystem physicsSystem;

    for (int i = 0; i < 120; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f);
        physicsSystem.syncToEntities(&scene);

        glm::vec3 const delta    = dynamicTransform.translation - staticTransform.translation;
        float const     distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        minDistance              = std::min(minDistance, distance);
    }

    EXPECT_GT(dynamicTransform.translation.y, startY);
    EXPECT_GT(dynamicBody.velocity.y, 0.0f);
    EXPECT_GT(minDistance, 2.0f) << "Bodies got unexpectedly close and likely collided";
}

TEST_F(CollisionTest, GivenStackedSettleScene_WhenSimulated_ThenDynamicBodySettlesWithoutBounce) {
    std::string const path = "assets/scenes/test/stacked_settle_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity staticEntity  = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
        if (rb.isStatic) {
            staticEntity = entity;
        } else {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(staticEntity != entt::null) << "No static body found in stacked_settle_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in stacked_settle_scene.json";

    auto& staticTransform  = scene.getRegistry().get<TransformComponent>(staticEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    JoltPhysicsSystem physicsSystem;

    bool sawContact = false;
    for (int i = 0; i < 300; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f);
        physicsSystem.syncToEntities(&scene);

        float const separationY = std::abs(dynamicTransform.translation.y - staticTransform.translation.y);
        if (separationY < 1.25f) {
            sawContact = true;
        }
    }

    float const finalSeparationY = std::abs(dynamicTransform.translation.y - staticTransform.translation.y);
    EXPECT_TRUE(sawContact) << "Dynamic body never reached the floor";
    EXPECT_NEAR(finalSeparationY, 1.0f, 0.6f) << "Settled center separation is not near contact distance";
    EXPECT_LT(std::abs(dynamicBody.velocity.y), 0.2f) << "Dynamic body did not settle to near-rest velocity";
}

TEST_F(CollisionTest, GivenHighSpeedTunnelingScene_WhenSimulated_ThenNoGhostThroughOccurs) {
    std::string const path = "assets/scenes/test/high_speed_tunneling_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity staticEntity  = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = scene.getRegistry().get<RigidBodyComponent>(entity);
        if (rb.isStatic) {
            staticEntity = entity;
        } else {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(staticEntity != entt::null) << "No static body found in high_speed_tunneling_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in high_speed_tunneling_scene.json";

    auto& staticTransform  = scene.getRegistry().get<TransformComponent>(staticEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    bool sawContact = false;
    bool sawRebound = false;

    JoltPhysicsSystem physicsSystem;

    for (int i = 0; i < 120; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f);
        physicsSystem.syncToEntities(&scene);

        float const separationY = std::abs(dynamicTransform.translation.y - staticTransform.translation.y);
        if (separationY < 1.25f) {
            sawContact = true;
        }
        if (sawContact && dynamicBody.velocity.y < 0.0f) {
            sawRebound = true;
            break;
        }
    }

    EXPECT_TRUE(sawContact) << "High-speed body never contacted the wall (possible tunneling)";
    EXPECT_TRUE(sawRebound) << "High-speed contact did not produce rebound response";
}

TEST_F(CollisionTest, GivenTriggerOverlapScene_WhenSimulated_ThenDynamicBodyPassesWithoutPhysicalBounce) {
    std::string const path = "assets/scenes/test/trigger_overlap_scene.json";

    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Missing scene file: " << path;
    }

    Scene           scene;
    SceneSerializer serializer(scene, resourceManager());
    ASSERT_TRUE(serializer.deserialize(path));

    entt::entity triggerEntity = entt::null;
    entt::entity dynamicEntity = entt::null;

    auto view = scene.getRegistry().view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb  = scene.getRegistry().get<RigidBodyComponent>(entity);
        auto& col = scene.getRegistry().get<ColliderComponent>(entity);
        if (rb.isStatic && col.isTrigger) {
            triggerEntity = entity;
        } else if (!rb.isStatic) {
            dynamicEntity = entity;
        }
    }

    ASSERT_TRUE(triggerEntity != entt::null) << "No trigger body found in trigger_overlap_scene.json";
    ASSERT_TRUE(dynamicEntity != entt::null) << "No dynamic body found in trigger_overlap_scene.json";

    auto& triggerTransform = scene.getRegistry().get<TransformComponent>(triggerEntity);
    auto& dynamicTransform = scene.getRegistry().get<TransformComponent>(dynamicEntity);
    auto& dynamicBody      = scene.getRegistry().get<RigidBodyComponent>(dynamicEntity);

    bool sawOverlap = false;

    JoltPhysicsSystem physicsSystem;

    for (int i = 0; i < 120; ++i) {
        physicsSystem.syncToEntities(&scene);
        physicsSystem.update(1.0f / 60.0f);
        physicsSystem.syncToEntities(&scene);

        float const separationY = std::abs(dynamicTransform.translation.y - triggerTransform.translation.y);
        if (separationY < 1.25f) {
            sawOverlap = true;
        }
    }

    EXPECT_TRUE(sawOverlap) << "Dynamic body never overlapped trigger volume";
    EXPECT_GT(dynamicTransform.translation.y, triggerTransform.translation.y + 1.0f)
        << "Dynamic body appears blocked by trigger volume";
    EXPECT_GT(dynamicBody.velocity.y, 0.0f) << "Dynamic body unexpectedly bounced off trigger";
}
