#include "Engine/Systems/PhysicsSystem.hpp"

#include <cassert>

namespace engine {

    PhysicsSystem::PhysicsSystem(Device& device) : device(device) {
        // Constructor implementation
    }

    void PhysicsSystem::update(FrameInfo& frameInfo) {
        auto& registry = frameInfo.scene->getRegistry();

        // Process all entities with both RigidBodyComponent and TransformComponent
        auto view = registry.view<RigidBodyComponent, TransformComponent>();
        for (auto entity : view) {
            auto [rigidBody, transform] = view.get<RigidBodyComponent, TransformComponent>(entity);

            // Skip static bodies
            if (rigidBody.isStatic) {
                continue;
            }

            // Apply forces first
            applyForces(rigidBody);

            // Integrate physics
            integrateRigidBody(rigidBody, transform);

            // Update transform from rigid body data
            updateTransformFromRigidBody(rigidBody, transform);
        }
    }

    void PhysicsSystem::applyForces(RigidBodyComponent& rigidBody) {
        if (!rigidBody.useGravity || rigidBody.isStatic) {
            return;
        }

        // Apply gravity force along -Y (downward in world space).
        rigidBody.acceleration.y -= kGravity * rigidBody.mass;
    }

    void PhysicsSystem::integrateRigidBody(RigidBodyComponent& rigidBody, TransformComponent& transform) {
        // Update velocity using acceleration (Euler integration)
        rigidBody.velocity.x += rigidBody.acceleration.x;
        rigidBody.velocity.y += rigidBody.acceleration.y;
        rigidBody.velocity.z += rigidBody.acceleration.z;

        // Apply angular velocity to rotation
        transform.rotation.x += rigidBody.angularVelocity.x;
        transform.rotation.y += rigidBody.angularVelocity.y;
        transform.rotation.z += rigidBody.angularVelocity.z;

        // Update position using velocity
        transform.translation.x += rigidBody.velocity.x;
        transform.translation.y += rigidBody.velocity.y;
        transform.translation.z += rigidBody.velocity.z;

        // Reset acceleration for next frame
        rigidBody.acceleration = glm::vec3(0.0f);
    }

    void PhysicsSystem::updateTransformFromRigidBody(const RigidBodyComponent& rigidBody, TransformComponent& transform) {
        // This method ensures that the transform is properly updated from physics data
        // In a more complex system, we might have additional logic here for constraints,
        // collision response, etc.
    }

}  // namespace engine