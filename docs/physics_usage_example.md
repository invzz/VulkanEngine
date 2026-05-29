/*
 * Physics System Usage Example
 *
 * This example demonstrates how to use the physics system in a Vulkan engine.
 * 
 * To use physics in your scene, you need to:
 * 1. Add both TransformComponent and RigidBodyComponent to entities
 * 2. The physics system will automatically process all entities with these components
 * 3. Physics calculations update the transform components directly
 *
 * Example usage:
 */

#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

// In your scene setup code:
void setupPhysicsExample() {
    // Create an entity with both components
    auto entity = scene->getRegistry().create();
    
    // Add transform component (required for rendering)
    auto& transform = scene->getRegistry().emplace<TransformComponent>(entity);
    transform.translation = glm::vec3(0.0f, 10.0f, 0.0f);  // Start 10 units above ground
    transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // Add rigid body component (physics properties)
    auto& rigidBody = scene->getRegistry().emplace<RigidBodyComponent>(entity);
    rigidBody.mass = 1.0f;
    rigidBody.velocity = glm::vec3(0.0f, 0.0f, 0.0f);     // Start at rest
    rigidBody.acceleration = glm::vec3(0.0f, 0.0f, 0.0f); // No initial acceleration
    rigidBody.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f); // No rotation
    rigidBody.isStatic = false;                           // Not static (will be affected by physics)
    rigidBody.useGravity = true;                          // Apply gravity
    
    // The physics system will automatically update this entity each frame
    // through the FrameInfo passed to PhysicsSystem::update(frameInfo);
}

// In your main game loop:
void updateFrame(FrameInfo& frameInfo) {
    // Update physics for all entities with RigidBodyComponent and TransformComponent
    PhysicsSystem::update(frameInfo);
    
    // The transform components of all physics-enabled entities are now updated
    // based on physics calculations (velocity, gravity, etc.)
}