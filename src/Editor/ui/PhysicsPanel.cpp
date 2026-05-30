#include "Editor/ui/PhysicsPanel.hpp"

#include <imgui.h>

#include <cstdint>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "entt/entity/entity.hpp"

namespace engine {

PhysicsPanel::PhysicsPanel(Scene& scene, bool* simulationRunning, bool* showColliderWireframes)
    : scene_(scene), simulationRunning_(simulationRunning), showColliderWireframes_(showColliderWireframes) {}

void PhysicsPanel::render(FrameInfo& frameInfo) {
    if (!visible_) return;

    if (ImGui::Begin("Physics", &visible_)) {
        if (simulationRunning_ != nullptr) {
            if (!*simulationRunning_) {
                if (ImGui::Button("Play Physics")) {
                    *simulationRunning_ = true;
                }
            } else {
                if (ImGui::Button("Pause Physics")) {
                    *simulationRunning_ = false;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Status: %s", *simulationRunning_ ? "Running" : "Stopped");
            ImGui::Separator();
        }

        if (showColliderWireframes_ != nullptr) {
            if (ImGui::Button(*showColliderWireframes_ ? "Hide Collider Wireframes" : "Show Collider Wireframes")) {
                *showColliderWireframes_ = !*showColliderWireframes_;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Collider Debug: %s", *showColliderWireframes_ ? "On" : "Off");
            ImGui::Separator();
        }

        if (frameInfo.selectedEntity != entt::null) {
            auto entity = frameInfo.selectedEntity;
            auto& registry = scene_.getRegistry();

            // Check if the entity already has a physics component
            bool hasRigidBody = registry.all_of<RigidBodyComponent>(entity);
            bool hasCollider = registry.all_of<ColliderComponent>(entity);

            ImGui::Text("Selected: Object %u", (uint32_t)entity);
            ImGui::Separator();

            // Add Physics Component button
            if (!hasRigidBody) {
                if (ImGui::Button("Add Rigid Body Component")) {
                    addPhysicsComponent(frameInfo);
                }
                ImGui::Spacing();
            } else {
                // Remove Rigid Body button
                if (ImGui::Button("Remove Rigid Body Component")) {
                    if (registry.all_of<RigidBodyComponent>(entity)) {
                        registry.remove<RigidBodyComponent>(entity);
                    }
                }
                ImGui::Spacing();

                // Edit physics properties
                editPhysicsProperties(frameInfo);
            }

            // Add Collider Component button
            if (!hasCollider) {
                if (ImGui::Button("Add Collider Component")) {
                    // Add collider component to entity
                    auto& collider = registry.emplace<ColliderComponent>(entity);
                    collider.shape = ColliderComponent::ShapeType::Box;
                    collider.size = glm::vec3(1.0f, 1.0f, 1.0f);
                    collider.radius = 0.5f;
                    collider.centerOffset = glm::vec3(0.0f);
                    collider.pendingShapeRebuild = true;

                    if (registry.all_of<ModelComponent>(entity)) {
                        auto& modelComp = registry.get<ModelComponent>(entity);
                        if (modelComp.model != nullptr) {
                            const AABB& localBounds = modelComp.model->getLocalBounds();
                            if (localBounds.isValid()) {
                                const glm::vec3 extents = glm::max(localBounds.extents(), glm::vec3(0.01f));
                                collider.size = glm::max(extents * 2.0f, glm::vec3(0.02f));
                                collider.centerOffset = localBounds.center();
                            }
                        }
                    }

                    if (registry.all_of<RigidBodyComponent>(entity)) {
                        registry.get<RigidBodyComponent>(entity).pendingBodyStateOverride = true;
                    }
                }
                ImGui::Spacing();
            } else {
                // Remove Collider button
                if (ImGui::Button("Remove Collider Component")) {
                    if (registry.all_of<ColliderComponent>(entity)) {
                        registry.remove<ColliderComponent>(entity);
                    }
                }
                ImGui::Spacing();

                editColliderProperties(frameInfo);
            }

            // Display current component information
            if (hasRigidBody) {
                auto& rigidBody = registry.get<RigidBodyComponent>(entity);
                ImGui::Text("Mass: %.2f", rigidBody.mass);
                ImGui::Text("Velocity: (%.2f, %.2f, %.2f)",
                           rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z);
                ImGui::Text("Acceleration: (%.2f, %.2f, %.2f)",
                           rigidBody.acceleration.x, rigidBody.acceleration.y, rigidBody.acceleration.z);
                ImGui::Text("Angular Velocity: (%.2f, %.2f, %.2f)",
                           rigidBody.angularVelocity.x, rigidBody.angularVelocity.y, rigidBody.angularVelocity.z);
                ImGui::Text("Static: %s", rigidBody.isStatic ? "Yes" : "No");
                ImGui::Text("Use Gravity: %s", rigidBody.useGravity ? "Yes" : "No");
            }

            if (hasCollider) {
                auto& collider = registry.get<ColliderComponent>(entity);
                std::string shapeStr;
                switch (collider.shape) {
                    case ColliderComponent::ShapeType::Box:
                        shapeStr = "Box";
                        break;
                    case ColliderComponent::ShapeType::Sphere:
                        shapeStr = "Sphere";
                        break;
                    default:
                        shapeStr = "Mesh";
                        break;
                }
                ImGui::Text("Collider Type: %s", shapeStr.c_str());
                if (collider.shape == ColliderComponent::ShapeType::Box || collider.shape == ColliderComponent::ShapeType::Sphere) {
                    ImGui::Text("Size: (%.2f, %.2f, %.2f)",
                               collider.size.x, collider.size.y, collider.size.z);
                }
            }

        } else {
            ImGui::TextDisabled("No object selected");
            ImGui::TextDisabled("Press Y/U to select objects");
            ImGui::TextDisabled("Press C to select camera");
        }
    }

    ImGui::End();
}

void PhysicsPanel::addPhysicsComponent(FrameInfo& frameInfo) {
    auto entity = frameInfo.selectedEntity;
    auto& registry = scene_.getRegistry();

    // Check if the entity has a TransformComponent (required for physics)
    if (!registry.all_of<TransformComponent>(entity)) {
        // If no transform, add one
        auto& transform = registry.emplace<TransformComponent>(entity);
        transform.translation = glm::vec3(0.0f);
        transform.rotation = glm::vec3(0.0f);
        transform.scale = glm::vec3(1.0f);
    }

    // Add rigid body component with default values
    auto& rigidBody = registry.emplace<RigidBodyComponent>(entity);
    rigidBody.mass = 1.0f;
    rigidBody.velocity = glm::vec3(0.0f);
    rigidBody.acceleration = glm::vec3(0.0f);
    rigidBody.angularVelocity = glm::vec3(0.0f);
    rigidBody.isStatic = false;
    rigidBody.useGravity = true;
    rigidBody.pendingBodyStateOverride = true;
}

void PhysicsPanel::editPhysicsProperties(FrameInfo& frameInfo) {
    auto entity = frameInfo.selectedEntity;
    auto& registry = scene_.getRegistry();

    if (!registry.all_of<RigidBodyComponent>(entity)) {
        return;
    }

    auto& rigidBody = registry.get<RigidBodyComponent>(entity);

    ImGui::Text("Edit Physics Properties:");

    bool bodyStateEdited = false;

    // Mass
    ImGui::DragFloat("Mass", &rigidBody.mass, 0.1f, 0.01f, 1000.0f);

    // Static checkbox
    ImGui::Checkbox("Static Body", &rigidBody.isStatic);

    // Use Gravity checkbox
    ImGui::Checkbox("Use Gravity", &rigidBody.useGravity);

    ImGui::Separator();

    // Velocity
    if (ImGui::CollapsingHeader("Velocity")) {
        bodyStateEdited |= ImGui::DragFloat("X##velocity", &rigidBody.velocity.x, 0.1f);
        bodyStateEdited |= ImGui::DragFloat("Y##velocity", &rigidBody.velocity.y, 0.1f);
        bodyStateEdited |= ImGui::DragFloat("Z##velocity", &rigidBody.velocity.z, 0.1f);
    }

    // Acceleration
    if (ImGui::CollapsingHeader("Acceleration")) {
        ImGui::DragFloat("X##acceleration", &rigidBody.acceleration.x, 0.1f);
        ImGui::DragFloat("Y##acceleration", &rigidBody.acceleration.y, 0.1f);
        ImGui::DragFloat("Z##acceleration", &rigidBody.acceleration.z, 0.1f);
    }

    // Angular Velocity
    if (ImGui::CollapsingHeader("Angular Velocity")) {
        bodyStateEdited |= ImGui::DragFloat("X##angular", &rigidBody.angularVelocity.x, 0.01f);
        bodyStateEdited |= ImGui::DragFloat("Y##angular", &rigidBody.angularVelocity.y, 0.01f);
        bodyStateEdited |= ImGui::DragFloat("Z##angular", &rigidBody.angularVelocity.z, 0.01f);
    }

    if (bodyStateEdited) {
        rigidBody.pendingBodyStateOverride = true;
    }
}

void PhysicsPanel::editColliderProperties(FrameInfo& frameInfo) {
    auto entity = frameInfo.selectedEntity;
    auto& registry = scene_.getRegistry();

    if (!registry.all_of<ColliderComponent>(entity)) {
        return;
    }

    auto& collider = registry.get<ColliderComponent>(entity);
    auto* rigidBody = registry.try_get<RigidBodyComponent>(entity);

    ImGui::Text("Edit Collider Properties:");

    bool colliderChanged = false;

    const char* shapeLabels[] = {"Sphere", "Box", "Capsule", "Mesh"};
    int currentShapeIndex = static_cast<int>(collider.shape);
    if (ImGui::Combo("Shape", &currentShapeIndex, shapeLabels, IM_ARRAYSIZE(shapeLabels))) {
        collider.shape = static_cast<ColliderComponent::ShapeType>(currentShapeIndex);
        colliderChanged = true;
    }

    colliderChanged |= ImGui::Checkbox("Is Trigger", &collider.isTrigger);
    colliderChanged |= ImGui::DragFloat3("Center Offset", &collider.centerOffset.x, 0.01f);

    switch (collider.shape) {
        case ColliderComponent::ShapeType::Box:
            colliderChanged |= ImGui::DragFloat3("Size", &collider.size.x, 0.05f, 0.01f, 1000.0f);
            break;
        case ColliderComponent::ShapeType::Sphere:
            colliderChanged |= ImGui::DragFloat("Radius", &collider.radius, 0.01f, 0.01f, 1000.0f);
            break;
        case ColliderComponent::ShapeType::Capsule:
            colliderChanged |= ImGui::DragFloat("Radius##capsule", &collider.radius, 0.01f, 0.01f, 1000.0f);
            colliderChanged |= ImGui::DragFloat("Height##capsule", &collider.size.y, 0.05f, 0.01f, 1000.0f);
            break;
        case ColliderComponent::ShapeType::Mesh:
            ImGui::TextDisabled("Mesh collider uses model triangles.");
            break;
    }

    if (registry.all_of<ModelComponent>(entity)) {
        auto& modelComp = registry.get<ModelComponent>(entity);
        if (modelComp.model != nullptr) {
            const AABB& localBounds = modelComp.model->getLocalBounds();
            if (localBounds.isValid()) {
                if (ImGui::Button("Fit Collider To Model Bounds")) {
                    const glm::vec3 extents = glm::max(localBounds.extents(), glm::vec3(0.01f));
                    collider.centerOffset = localBounds.center();

                    switch (collider.shape) {
                        case ColliderComponent::ShapeType::Sphere:
                            collider.radius = glm::max(extents.x, glm::max(extents.y, extents.z));
                            break;
                        case ColliderComponent::ShapeType::Capsule:
                            collider.radius = glm::max(extents.x, extents.z);
                            collider.size.y = glm::max(extents.y * 2.0f, collider.radius * 2.0f);
                            break;
                        case ColliderComponent::ShapeType::Mesh:
                            // Mesh colliders already match mesh geometry; keep current shape.
                            break;
                        case ColliderComponent::ShapeType::Box:
                        default:
                            collider.size = glm::max(extents * 2.0f, glm::vec3(0.02f));
                            break;
                    }

                    colliderChanged = true;
                }
            }
        }
    }

    if (colliderChanged) {
        collider.pendingShapeRebuild = true;
        if (rigidBody != nullptr) {
            rigidBody->pendingBodyStateOverride = true;
        }
    }

    ImGui::Separator();
}

} // namespace engine