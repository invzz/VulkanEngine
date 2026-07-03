#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {

    PhysicsSystem::PhysicsSystem(Device& device) : device(device) {
    }

    void PhysicsSystem::update(FrameInfo& frameInfo) {
        auto& registry = frameInfo.scene->getRegistry();

        auto view = registry.view<RigidBodyComponent, TransformComponent>();
        for (auto entity : view) {
            auto [rigidBody, transform] = view.get<RigidBodyComponent, TransformComponent>(entity);

            if (rigidBody.isStatic) {
                continue;
            }

            applyForces(rigidBody);

            integrateRigidBody(rigidBody, transform);

            updateTransformFromRigidBody(rigidBody, transform);
        }
    }

    void PhysicsSystem::applyForces(RigidBodyComponent& rigidBody) {
        if (!rigidBody.useGravity || rigidBody.isStatic) {
            return;
        }

        rigidBody.acceleration.y -= kGravity * rigidBody.mass;
    }

    void PhysicsSystem::integrateRigidBody(RigidBodyComponent& rigidBody, TransformComponent& transform) {
        rigidBody.velocity.x += rigidBody.acceleration.x;
        rigidBody.velocity.y += rigidBody.acceleration.y;
        rigidBody.velocity.z += rigidBody.acceleration.z;

        transform.rotation.x += rigidBody.angularVelocity.x;
        transform.rotation.y += rigidBody.angularVelocity.y;
        transform.rotation.z += rigidBody.angularVelocity.z;

        transform.translation.x += rigidBody.velocity.x;
        transform.translation.y += rigidBody.velocity.y;
        transform.translation.z += rigidBody.velocity.z;

        rigidBody.acceleration = glm::vec3(0.0f);
    }

    void PhysicsSystem::updateTransformFromRigidBody(const RigidBodyComponent& rigidBody, TransformComponent& transform) {
    }

}  // namespace engine