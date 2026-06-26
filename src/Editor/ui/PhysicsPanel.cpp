#include "Editor/ui/PhysicsPanel.hpp"

#include <cstdint>
#include <string>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "Editor/ui/UI.hpp"

#include "entt/entity/entity.hpp"

namespace engine {

    PhysicsPanel::PhysicsPanel(Scene& scene, bool* simulationRunning, bool* showColliderWireframes, bool* solidGroundEnabled,
        JoltPhysicsSystem* joltPhysicsSystem)
        : scene_(scene), simulationRunning_(simulationRunning), showColliderWireframes_(showColliderWireframes), solidGroundEnabled_(solidGroundEnabled), joltPhysicsSystem_(joltPhysicsSystem) {}

    void PhysicsPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        if (simulationRunning_ != nullptr) {
            if (!*simulationRunning_) {
                if (ui::UI::Button("Play Physics##physics_play")) {
                    *simulationRunning_ = true;
                }
            } else {
                if (ui::UI::Button("Pause Physics##physics_pause")) {
                    *simulationRunning_ = false;
                }
            }
            std::string statusText = std::string(*simulationRunning_ ? "Status: Running" : "Status: Stopped");
            ui::UI::TextDisabled(statusText.c_str());
            ui::UI::Separator();
        }

        if (showColliderWireframes_ != nullptr) {
            std::string wireframeText = *showColliderWireframes_ ? "Hide Collider Wireframes" : "Show Collider Wireframes";
            if (ui::UI::Button(wireframeText.c_str())) {
                *showColliderWireframes_ = !*showColliderWireframes_;
            }
            std::string wireframeStatus = std::string(*showColliderWireframes_ ? "Collider Debug: On" : "Collider Debug: Off");
            ui::UI::TextDisabled(wireframeStatus.c_str());
            ui::UI::Separator();
        }

        if (solidGroundEnabled_ != nullptr) {
            std::string groundText = *solidGroundEnabled_ ? "Disable Solid Ground" : "Enable Solid Ground";
            if (ui::UI::Button(groundText.c_str())) {
                *solidGroundEnabled_ = !*solidGroundEnabled_;
                if (joltPhysicsSystem_ != nullptr) {
                    joltPhysicsSystem_->setGroundEnabled(*solidGroundEnabled_);
                }
            }
            std::string groundStatus = std::string(*solidGroundEnabled_ ? "Ground Plane: On" : "Ground Plane: Off");
            ui::UI::TextDisabled(groundStatus.c_str());
            ui::UI::Separator();
        }

        if (frameInfo.selectedEntity != entt::null) {
            auto  entity   = frameInfo.selectedEntity;
            auto& registry = scene_.getRegistry();

            // Check if the entity already has a physics component
            bool hasRigidBody = registry.all_of<RigidBodyComponent>(entity);
            bool hasCollider  = registry.all_of<ColliderComponent>(entity);

            std::string selectedText = "Selected: Object " + std::to_string((uint32_t) entity);
            ui::UI::TextColored(selectedText.c_str(), ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ui::UI::Separator();

            // Add Physics Component button
            if (!hasRigidBody) {
                if (ui::UI::Button("Add Rigid Body Component##physics_add_rb")) {
                    addPhysicsComponent(frameInfo);
                }
            } else {
                // Remove Rigid Body button
                if (ui::UI::Button("Remove Rigid Body Component##physics_remove_rb")) {
                    if (registry.all_of<RigidBodyComponent>(entity)) {
                        registry.remove<RigidBodyComponent>(entity);
                    }
                }

                // Edit physics properties
                editPhysicsProperties(frameInfo);
            }

            // Add Collider Component button
            if (!hasCollider) {
                if (ui::UI::Button("Add Collider Component##physics_add_collider")) {
                    // Add collider component to entity
                    auto& collider               = registry.emplace<ColliderComponent>(entity);
                    collider.shape               = ColliderComponent::ShapeType::Box;
                    collider.size                = glm::vec3(1.0f, 1.0f, 1.0f);
                    collider.radius              = 0.5f;
                    collider.centerOffset        = glm::vec3(0.0f);
                    collider.pendingShapeRebuild = true;

                    if (registry.all_of<ModelComponent>(entity)) {
                        auto& modelComp = registry.get<ModelComponent>(entity);
                        if (modelComp.model != nullptr) {
                            const AABB& localBounds = modelComp.model->getLocalBounds();
                            if (localBounds.isValid()) {
                                const glm::vec3 extents = glm::max(localBounds.extents(), glm::vec3(0.01f));
                                collider.size           = glm::max(extents * 2.0f, glm::vec3(0.02f));
                                collider.centerOffset   = localBounds.center();
                            }
                        }
                    }

                    if (registry.all_of<RigidBodyComponent>(entity)) {
                        registry.get<RigidBodyComponent>(entity).pendingBodyStateOverride = true;
                    }
                }
            } else {
                // Remove Collider button
                if (ui::UI::Button("Remove Collider Component##physics_remove_collider")) {
                    if (registry.all_of<ColliderComponent>(entity)) {
                        registry.remove<ColliderComponent>(entity);
                    }
                }

                editColliderProperties(frameInfo);
            }

            // Display current component information
            if (hasRigidBody) {
                auto& rigidBody = registry.get<RigidBodyComponent>(entity);
                std::string massText = "Mass: " + std::to_string(rigidBody.mass).substr(0, 5);
                ui::UI::TextDisabled(massText.c_str());
                std::string velText = "Velocity: (" + std::to_string(rigidBody.velocity.x).substr(0, 5) + ", " +
                                      std::to_string(rigidBody.velocity.y).substr(0, 5) + ", " +
                                      std::to_string(rigidBody.velocity.z).substr(0, 5) + ")";
                ui::UI::TextDisabled(velText.c_str());
                std::string accText = "Acceleration: (" + std::to_string(rigidBody.acceleration.x).substr(0, 5) + ", " +
                                      std::to_string(rigidBody.acceleration.y).substr(0, 5) + ", " +
                                      std::to_string(rigidBody.acceleration.z).substr(0, 5) + ")";
                ui::UI::TextDisabled(accText.c_str());
                std::string angVelText = "Angular Velocity: (" + std::to_string(rigidBody.angularVelocity.x).substr(0, 5) + ", " +
                                         std::to_string(rigidBody.angularVelocity.y).substr(0, 5) + ", " +
                                         std::to_string(rigidBody.angularVelocity.z).substr(0, 5) + ")";
                ui::UI::TextDisabled(angVelText.c_str());
                std::string staticText = std::string(rigidBody.isStatic ? "Static: Yes" : "Static: No");
                ui::UI::TextDisabled(staticText.c_str());
                std::string gravityText = std::string(rigidBody.useGravity ? "Use Gravity: Yes" : "Use Gravity: No");
                ui::UI::TextDisabled(gravityText.c_str());
            }

            if (hasCollider) {
                auto&       collider = registry.get<ColliderComponent>(entity);
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
                std::string colliderTypeText = "Collider Type: " + shapeStr;
                ui::UI::TextDisabled(colliderTypeText.c_str());
                if (collider.shape == ColliderComponent::ShapeType::Box || collider.shape == ColliderComponent::ShapeType::Sphere) {
                    std::string sizeText = "Size: (" + std::to_string(collider.size.x).substr(0, 5) + ", " +
                                           std::to_string(collider.size.y).substr(0, 5) + ", " +
                                           std::to_string(collider.size.z).substr(0, 5) + ")";
                    ui::UI::TextDisabled(sizeText.c_str());
                }
            }

        } else {
            ui::UI::TextDisabled("No object selected");
            ui::UI::TextDisabled("Press Y/U to select objects");
            ui::UI::TextDisabled("Press C to select camera");
        }
    }

    void PhysicsPanel::addPhysicsComponent(FrameInfo& frameInfo) {
        auto  entity   = frameInfo.selectedEntity;
        auto& registry = scene_.getRegistry();

        // Check if the entity has a TransformComponent (required for physics)
        if (!registry.all_of<TransformComponent>(entity)) {
            // If no transform, add one
            auto& transform       = registry.emplace<TransformComponent>(entity);
            transform.translation = glm::vec3(0.0f);
            transform.rotation    = glm::vec3(0.0f);
            transform.scale       = glm::vec3(1.0f);
        }

        // Add rigid body component with default values
        auto& rigidBody                    = registry.emplace<RigidBodyComponent>(entity);
        rigidBody.mass                     = 1.0f;
        rigidBody.velocity                 = glm::vec3(0.0f);
        rigidBody.acceleration             = glm::vec3(0.0f);
        rigidBody.angularVelocity          = glm::vec3(0.0f);
        rigidBody.isStatic                 = false;
        rigidBody.useGravity               = true;
        rigidBody.pendingBodyStateOverride = true;
    }

    void PhysicsPanel::editPhysicsProperties(FrameInfo& frameInfo) {
        auto  entity   = frameInfo.selectedEntity;
        auto& registry = scene_.getRegistry();

        if (!registry.all_of<RigidBodyComponent>(entity)) {
            return;
        }

        auto& rigidBody = registry.get<RigidBodyComponent>(entity);

        bool bodyStateEdited = false;

        // Mass
        ui::UI::DragFloat("Mass##phys_mass", &rigidBody.mass, 0.1f, 0.01f, 1000.0f);

        // Static checkbox
        if (ui::UI::Checkbox("Static Body##phys_static", &rigidBody.isStatic)) {
            bodyStateEdited = true;
        }

        // Use Gravity checkbox
        if (ui::UI::Checkbox("Use Gravity##phys_gravity", &rigidBody.useGravity)) {
            bodyStateEdited = true;
        }

        ui::UI::Separator();

        // Velocity
        if (ui::UI::Section("Velocity")) {
            bool edited = false;
            edited |= ui::UI::DragFloat("X##phys_vx", &rigidBody.velocity.x, 0.1f);
            edited |= ui::UI::DragFloat("Y##phys_vy", &rigidBody.velocity.y, 0.1f);
            edited |= ui::UI::DragFloat("Z##phys_vz", &rigidBody.velocity.z, 0.1f);
            bodyStateEdited |= edited;
        }

        // Acceleration
        if (ui::UI::Section("Acceleration")) {
            bool edited = false;
            edited |= ui::UI::DragFloat("X##phys_ax", &rigidBody.acceleration.x, 0.1f);
            edited |= ui::UI::DragFloat("Y##phys_ay", &rigidBody.acceleration.y, 0.1f);
            edited |= ui::UI::DragFloat("Z##phys_az", &rigidBody.acceleration.z, 0.1f);
            bodyStateEdited |= edited;
        }

        // Angular Velocity
        if (ui::UI::Section("Angular Velocity")) {
            bool edited = false;
            edited |= ui::UI::DragFloat("X##phys_avx", &rigidBody.angularVelocity.x, 0.01f);
            edited |= ui::UI::DragFloat("Y##phys_avy", &rigidBody.angularVelocity.y, 0.01f);
            edited |= ui::UI::DragFloat("Z##phys_avz", &rigidBody.angularVelocity.z, 0.01f);
            bodyStateEdited |= edited;
        }

        if (bodyStateEdited) {
            rigidBody.pendingBodyStateOverride = true;
        }
    }

    void PhysicsPanel::editColliderProperties(FrameInfo& frameInfo) {
        auto  entity   = frameInfo.selectedEntity;
        auto& registry = scene_.getRegistry();

        if (!registry.all_of<ColliderComponent>(entity)) {
            return;
        }

        auto& collider  = registry.get<ColliderComponent>(entity);
        auto* rigidBody = registry.try_get<RigidBodyComponent>(entity);

        bool colliderChanged = false;

        const char* shapeLabels[]     = {"Sphere", "Box", "Capsule", "Mesh"};
        int         currentShapeIndex = static_cast<int>(collider.shape);
        if (ui::UI::Combo("Shape##phys_shape", &currentShapeIndex, shapeLabels, IM_ARRAYSIZE(shapeLabels))) {
            collider.shape  = static_cast<ColliderComponent::ShapeType>(currentShapeIndex);
            colliderChanged = true;
        }

        if (ui::UI::Checkbox("Is Trigger##phys_trigger", &collider.isTrigger)) {
            colliderChanged = true;
        }
        if (ui::UI::DragFloat3("Center Offset##phys_center", &collider.centerOffset.x, 0.01f)) {
            colliderChanged = true;
        }

        switch (collider.shape) {
            case ColliderComponent::ShapeType::Box:
                if (ui::UI::DragFloat3("Size##phys_box_size", &collider.size.x, 0.05f, 0.01f, 1000.0f)) {
                    colliderChanged = true;
                }
                break;
            case ColliderComponent::ShapeType::Sphere:
                if (ui::UI::DragFloat("Radius##phys_sphere_radius", &collider.radius, 0.01f, 0.01f, 1000.0f)) {
                    colliderChanged = true;
                }
                break;
            case ColliderComponent::ShapeType::Capsule:
                if (ui::UI::DragFloat("Radius##phys_capsule_radius", &collider.radius, 0.01f, 0.01f, 1000.0f)) {
                    colliderChanged = true;
                }
                if (ui::UI::DragFloat("Height##phys_capsule_height", &collider.size.y, 0.05f, 0.01f, 1000.0f)) {
                    colliderChanged = true;
                }
                break;
            case ColliderComponent::ShapeType::Mesh:
                ui::UI::TextDisabled("Mesh collider uses model triangles.");
                break;
        }

        if (registry.all_of<ModelComponent>(entity)) {
            auto& modelComp = registry.get<ModelComponent>(entity);
            if (modelComp.model != nullptr) {
                const AABB& localBounds = modelComp.model->getLocalBounds();
                if (localBounds.isValid()) {
                    if (ui::UI::Button("Fit Collider To Model Bounds##phys_fit_bounds")) {
                        const glm::vec3 extents = glm::max(localBounds.extents(), glm::vec3(0.01f));
                        collider.centerOffset   = localBounds.center();

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

        ui::UI::Separator();
    }

}  // namespace engine
