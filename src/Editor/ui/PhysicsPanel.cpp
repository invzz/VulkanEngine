#include "Editor/ui/PhysicsPanel.hpp"

#include <imgui.h>

#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "Editor/ui/UI.hpp"

namespace engine {

    PhysicsPanel::PhysicsPanel(EngineState& state)
        : state_(state) {}

    void PhysicsPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        // Physics panel renders inside the dockspace like every other panel.
        // The window title "Physics" matches the registry key used in app.cpp.
        ui::UI::PushThemeStyle();
        if (ImGui::Begin("Physics", &visible_)) {
            bool& simRunning  = state_.physicsRunning();
            bool& showWires   = state_.showColliderWireframes();
            bool& solidGround = state_.solidGround();
            auto* jolt        = state_.systemPtr<JoltPhysicsSystem>();

            if (ui::UI::Button(simRunning ? "Pause Physics##physics_pause" : "Play Physics##physics_play")) {
                simRunning = !simRunning;
            }
            std::string statusText = simRunning ? "Status: Running" : "Status: Stopped";
            ui::UI::TextDisabled(statusText.c_str());
            ui::UI::Separator();

            if (ui::UI::Button(showWires ? "Hide Collider Wireframes" : "Show Collider Wireframes")) {
                showWires = !showWires;
            }
            std::string wireStatus = showWires ? "Collider Debug: On" : "Collider Debug: Off";
            ui::UI::TextDisabled(wireStatus.c_str());
            ui::UI::Separator();

            if (ui::UI::Button(solidGround ? "Disable Solid Ground" : "Enable Solid Ground")) {
                solidGround = !solidGround;
                if (jolt)
                    jolt->setGroundEnabled(solidGround);
            }
            std::string groundStatus = solidGround ? "Ground Plane: On" : "Ground Plane: Off";
            ui::UI::TextDisabled(groundStatus.c_str());
            ui::UI::Separator();

            if (frameInfo.selectedEntity != entt::null) {
                auto  entity   = frameInfo.selectedEntity;
                auto& registry = state_.scene().getRegistry();

                bool hasRigidBody = registry.all_of<RigidBodyComponent>(entity);
                bool hasCollider  = registry.all_of<ColliderComponent>(entity);

                std::string selectedText = "Selected: Object " + std::to_string((uint32_t) entity);
                ui::UI::TextColored(selectedText.c_str(), ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ui::UI::Separator();

                if (!hasRigidBody) {
                    if (ui::UI::Button("Add Rigid Body Component##physics_add_rb"))
                        addPhysicsComponent(frameInfo);
                } else {
                    if (ui::UI::Button("Remove Rigid Body Component##physics_remove_rb")) {
                        if (registry.all_of<RigidBodyComponent>(entity))
                            registry.remove<RigidBodyComponent>(entity);
                    }
                    editPhysicsProperties(frameInfo);
                }

                if (!hasCollider) {
                    if (ui::UI::Button("Add Collider Component##physics_add_collider")) {
                        auto& collider               = registry.emplace<ColliderComponent>(entity);
                        collider.shape               = ColliderComponent::ShapeType::Box;
                        collider.size                = glm::vec3(1.0f);
                        collider.radius              = 0.5f;
                        collider.centerOffset        = glm::vec3(0.0f);
                        collider.pendingShapeRebuild = true;
                        if (registry.all_of<ModelComponent>(entity)) {
                            auto& mc = registry.get<ModelComponent>(entity);
                            if (mc.model) {
                                const AABB& lb = mc.model->getLocalBounds();
                                if (lb.isValid()) {
                                    glm::vec3 ext         = glm::max(lb.extents(), glm::vec3(0.01f));
                                    collider.size         = glm::max(ext * 2.0f, glm::vec3(0.02f));
                                    collider.centerOffset = lb.center();
                                }
                            }
                        }
                        if (registry.all_of<RigidBodyComponent>(entity))
                            registry.get<RigidBodyComponent>(entity).pendingBodyStateOverride = true;
                    }
                } else {
                    if (ui::UI::Button("Remove Collider Component##physics_remove_collider")) {
                        if (registry.all_of<ColliderComponent>(entity))
                            registry.remove<ColliderComponent>(entity);
                    }
                    editColliderProperties(frameInfo);
                }

                if (hasRigidBody) {
                    auto& rb = registry.get<RigidBodyComponent>(entity);
                    ui::UI::TextDisabled(("Mass: " + std::to_string(rb.mass).substr(0, 5)).c_str());
                }
                if (hasCollider) {
                    auto&       col      = registry.get<ColliderComponent>(entity);
                    const char* shapes[] = {"Sphere", "Box", "Capsule", "Mesh"};
                    ui::UI::TextDisabled(("Collider: " + std::string(shapes[static_cast<int>(col.shape)])).c_str());
                }
            } else {
                ui::UI::TextDisabled("No object selected");
            }
        }
        ImGui::End();
        ui::UI::PopThemeStyle();
    }

    void PhysicsPanel::addPhysicsComponent(FrameInfo& frameInfo) {
        auto& registry = state_.scene().getRegistry();
        auto  entity   = frameInfo.selectedEntity;
        if (!registry.all_of<TransformComponent>(entity))
            registry.emplace<TransformComponent>(entity);
        auto& rb    = registry.emplace<RigidBodyComponent>(entity);
        rb.mass     = 1.0f;
        rb.velocity = rb.acceleration = rb.angularVelocity = glm::vec3(0.0f);
        rb.isStatic                                        = false;
        rb.useGravity                                      = true;
        rb.pendingBodyStateOverride                        = true;
    }

    void PhysicsPanel::editPhysicsProperties(FrameInfo& frameInfo) {
        auto& registry = state_.scene().getRegistry();
        auto  entity   = frameInfo.selectedEntity;
        if (!registry.all_of<RigidBodyComponent>(entity))
            return;
        auto& rb     = registry.get<RigidBodyComponent>(entity);
        bool  edited = false;
        ui::UI::DragFloat("Mass##phys_mass", &rb.mass, 0.1f, 0.01f, 1000.0f);
        edited |= ui::UI::Checkbox("Static Body##phys_static", &rb.isStatic);
        edited |= ui::UI::Checkbox("Use Gravity##phys_gravity", &rb.useGravity);
        ui::UI::Separator();
        if (ui::UI::Section("Velocity")) {
            edited |= ui::UI::DragFloat("X##phys_vx", &rb.velocity.x, 0.1f);
            edited |= ui::UI::DragFloat("Y##phys_vy", &rb.velocity.y, 0.1f);
            edited |= ui::UI::DragFloat("Z##phys_vz", &rb.velocity.z, 0.1f);
        }
        if (ui::UI::Section("Acceleration")) {
            edited |= ui::UI::DragFloat("X##phys_ax", &rb.acceleration.x, 0.1f);
            edited |= ui::UI::DragFloat("Y##phys_ay", &rb.acceleration.y, 0.1f);
            edited |= ui::UI::DragFloat("Z##phys_az", &rb.acceleration.z, 0.1f);
        }
        if (ui::UI::Section("Angular Velocity")) {
            edited |= ui::UI::DragFloat("X##phys_avx", &rb.angularVelocity.x, 0.01f);
            edited |= ui::UI::DragFloat("Y##phys_avy", &rb.angularVelocity.y, 0.01f);
            edited |= ui::UI::DragFloat("Z##phys_avz", &rb.angularVelocity.z, 0.01f);
        }
        if (edited)
            rb.pendingBodyStateOverride = true;
    }

    void PhysicsPanel::editColliderProperties(FrameInfo& frameInfo) {
        auto& registry = state_.scene().getRegistry();
        auto  entity   = frameInfo.selectedEntity;
        if (!registry.all_of<ColliderComponent>(entity))
            return;
        auto& col     = registry.get<ColliderComponent>(entity);
        auto* rb      = registry.try_get<RigidBodyComponent>(entity);
        bool  changed = false;

        const char* shapeLabels[] = {"Sphere", "Box", "Capsule", "Mesh"};
        int         si            = static_cast<int>(col.shape);
        if (ui::UI::Combo("Shape##phys_shape", &si, shapeLabels, 4)) {
            col.shape = static_cast<ColliderComponent::ShapeType>(si);
            changed   = true;
        }
        changed |= ui::UI::Checkbox("Is Trigger##phys_trigger", &col.isTrigger);
        changed |= ui::UI::DragFloat3("Center Offset##phys_center", &col.centerOffset.x, 0.01f);
        switch (col.shape) {
            case ColliderComponent::ShapeType::Box:
                changed |= ui::UI::DragFloat3("Size##phys_box_size", &col.size.x, 0.05f, 0.01f, 1000.0f);
                break;
            case ColliderComponent::ShapeType::Sphere:
                changed |= ui::UI::DragFloat("Radius##phys_sphere_radius", &col.radius, 0.01f, 0.01f, 1000.0f);
                break;
            case ColliderComponent::ShapeType::Capsule:
                changed |= ui::UI::DragFloat("Radius##phys_capsule_radius", &col.radius, 0.01f, 0.01f, 1000.0f);
                changed |= ui::UI::DragFloat("Height##phys_capsule_height", &col.size.y, 0.05f, 0.01f, 1000.0f);
                break;
            default:
                break;
        }
        if (changed) {
            col.pendingShapeRebuild = true;
            if (rb)
                rb->pendingBodyStateOverride = true;
        }
    }

}  // namespace engine