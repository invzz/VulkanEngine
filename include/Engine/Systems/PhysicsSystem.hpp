#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_PHYSICSSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_PHYSICSSYSTEM_HPP

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

class PhysicsSystem {
public:
    PhysicsSystem(Device& device);
    ~PhysicsSystem() = default;

    // delete copy operations
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    static void update(FrameInfo& frameInfo);

private:
    Device& device;

    // Physics constants
    static constexpr float kGravity = 9.81f;
    static constexpr float kTimeStep = 1.0f / 60.0f; // 60 FPS time step

    // Physics simulation helpers
    static void integrateRigidBody(RigidBodyComponent& rigidBody, TransformComponent& transform);
    static void applyForces(RigidBodyComponent& rigidBody);
    static void updateTransformFromRigidBody(const RigidBodyComponent& rigidBody, TransformComponent& transform);
};

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_PHYSICSSYSTEM_HPP