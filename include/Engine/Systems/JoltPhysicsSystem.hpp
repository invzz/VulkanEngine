#ifndef VULKANENGINE_JOLTPHYSICSSYSTEM_HPP
#define VULKANENGINE_JOLTPHYSICSSYSTEM_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Jolt/Jolt.h"

#include "Jolt/Core/Factory.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h"
#include "Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/ObjectLayerPairFilterTable.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/RegisterTypes.h"

#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

    class Scene;

    class JoltPhysicsSystem {
       public:
        JoltPhysicsSystem();
        ~JoltPhysicsSystem();

        JoltPhysicsSystem(const JoltPhysicsSystem&)            = delete;
        JoltPhysicsSystem& operator=(const JoltPhysicsSystem&) = delete;

        /**
     * @brief Update the physics world for the given timestep.
     * @param frameTime Delta time in seconds.
     * @param maxSubSteps Maximum number of sub-steps to prevent spiral of death.
     * @param subStepTime Fixed sub-step time (0 = auto).
     */
        void update(float frameTime, int maxSubSteps = 2, float subStepTime = 0.0f);

        /**
     * @brief Sync physics bodies to ECS components.
     * Called AFTER update() so transforms are read back from Jolt.
     */
        void syncToEntities(Scene* scene);

        /**
     * @brief Add or update a physics body from ECS components.
     */
        void syncEntity(entt::registry& registry, entt::entity entity);

        /**
     * @brief Remove a physics body for an entity.
     */
        void removeEntity(entt::entity e);

        /**
     * @brief Clear all physics bodies.
     */
        void clear();

        /**
     * @brief Enable or disable the solid ground body at y = 0.
     */
        void setGroundEnabled(bool enabled);

        /**
     * @brief Check whether the solid ground body is currently enabled.
     */
        [[nodiscard]] bool isGroundEnabled() const {
            return groundEnabled_;
        }

        /**
     * @brief Raycast against the physics world.
     * @param origin Start point
     * @param direction Normalized direction
     * @param maxDistance Maximum distance to test
     * @return Hit point or glm::vec3(0,0,0) if no hit
     */
        glm::vec3 raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;

        /**
     * @brief Get the underlying JPH::PhysicsSystem pointer.
     */
        JPH::PhysicsSystem* getPhysicsSystem() {
            return physicsSystem_.get();
        }

       private:
        void createGroundBody();

        /**
     * @brief Create a Jolt collision shape from ColliderComponent.
     */
        static std::unique_ptr<JPH::BodyCreationSettings> createCollisionShape(
            const TransformComponent&       transform,
            const RigidBodyComponent&       rigidBody,
            const ColliderComponent&        collider,
            const PhysicsMaterialComponent* material);

        /**
    * @brief Convert glm::vec3 to JPH::Vec3.
     */
        static JPH::Vec3 toJolt(const glm::vec3& v);
        static glm::vec3 toGlm(const JPH::Vec3& v);
        static glm::quat toGlm(const JPH::Quat& q);
        static JPH::Quat toJolt(const glm::quat& q);

        static constexpr JPH::ObjectLayer cNonMovingObjectLayer = 0;
        static constexpr JPH::ObjectLayer cMovingObjectLayer    = 1;
        static constexpr uint32_t         cNumObjectLayers      = 2;
        static constexpr uint32_t         cNumBroadPhaseLayers  = 2;

        // Jolt core objects
        std::unique_ptr<JPH::Factory>             factory_;
        std::unique_ptr<JPH::TempAllocatorImpl>   tempAllocator_;
        std::unique_ptr<JPH::JobSystemThreadPool> jobSystem_;
        std::unique_ptr<JPH::PhysicsSystem>       physicsSystem_;

        // Collision layer routing used by PhysicsSystem::Init.
        std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable>      broadPhaseLayerInterface_;
        std::unique_ptr<JPH::ObjectLayerPairFilterTable>         objectLayerPairFilter_;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhaseLayerFilter_;

        // Registered types flag
        bool typesRegistered_{false};

        // Map entity ID → Jolt body ID for sync
        struct BodySyncInfo {
            JPH::BodyID                  bodyID;
            ColliderComponent::ShapeType shapeType{ColliderComponent::ShapeType::Sphere};
        };
        std::unordered_map<uint32_t, BodySyncInfo> bodyMap_;
        JPH::BodyID                                groundBodyID_;
        bool                                       groundEnabled_{true};
    };

}  // namespace engine

#endif  // VULKANENGINE_JOLTPHYSICSSYSTEM_HPP
