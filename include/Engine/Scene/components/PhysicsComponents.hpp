#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_PHYSICSCOMPONENTS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_PHYSICSCOMPONENTS_HPP
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Component.hpp"
namespace engine {
    struct RigidBodyComponent {
        float     mass{1.0f};
        glm::vec3 velocity{};
        glm::vec3 acceleration{};
        glm::vec3 angularVelocity{};
        bool      isStatic{false};
        bool      useGravity{true};
        float     friction{0.5f};
        float     restitution{0.3f};
        bool      pendingBodyStateOverride{false};
        enum class PhysicsMode {
            Dynamic,
            Kinematic,
            Static
        };
        PhysicsMode mode{PhysicsMode::Dynamic};
    };
    struct ColliderComponent {
        enum class ShapeType {
            Sphere,
            Box,
            Capsule,
            Mesh
        };
        ShapeType shape{ShapeType::Sphere};
        glm::vec3 size{1.0f, 1.0f, 1.0f};
        float     radius{0.5f};
        glm::vec3 centerOffset{0.0f, 0.0f, 0.0f};
        bool      isTrigger{false};
        bool      pendingShapeRebuild{false};
        uint32_t  collisionGroup{0};
        uint32_t  collisionMask{0xFFFFFFFF};
    };
    struct PhysicsMaterialComponent {
        float friction{0.5f};
        float restitution{0.3f};
        float density{1.0f};
        float dynamicFriction{0.4f};
        float staticFriction{0.6f};
        float damping{0.0f};
        float angularDamping{0.0f};
    };
}  // namespace engine
#endif