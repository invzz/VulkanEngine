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

        // For future extension - allow different physics simulation modes
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
        glm::vec3 size{1.0f, 1.0f, 1.0f};  // For box and capsule shapes
        float     radius{0.5f};            // For sphere shape
        glm::vec3 centerOffset{0.0f, 0.0f, 0.0f};

        // For mesh collider - reference to mesh data or mesh ID
        // This would be populated by a mesh loading system
        bool isTrigger{false};
        bool pendingShapeRebuild{false};

        // Collision filtering
        uint32_t collisionGroup{0};
        uint32_t collisionMask{0xFFFFFFFF};
    };

    struct PhysicsMaterialComponent {
        float friction{0.5f};
        float restitution{0.3f};
        float density{1.0f};
        float dynamicFriction{0.4f};
        float staticFriction{0.6f};

        // Additional physical properties
        float damping{0.0f};         // Linear damping
        float angularDamping{0.0f};  // Angular damping
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_PHYSICSCOMPONENTS_HPP