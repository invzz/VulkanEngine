#include "Editor/ui/Panels/PhysicsPanel.hpp"

#include <imgui.h>

#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "Editor/ui/SelectionResolve.hpp"
#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
namespace engine {
    PhysicsPanel::PhysicsPanel(EngineState& state)
        : state_(state) {}
    void PhysicsPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }
        ui::UI::PushThemeStyle();
        if (ImGui::Begin("Physics", &visible_)) {
            bool& simRunning  = state_.physicsRunning();
            bool& showWires   = state_.showColliderWireframes();
            bool& solidGround = state_.solidGround();
            auto* jolt        = state_.systemPtr<JoltPhysicsSystem>();
            ui::UI::BeginSurface("physics_runtime", "Simulation", "Global physics runtime controls");
            if (ui::UI::PrimaryButton((std::string(simRunning ? ICON_FA_PAUSE : ICON_FA_PLAY) + " Toggle Simulation##physics_toggle").c_str())) {
                simRunning = !simRunning;
            }
            std::string statusText = simRunning ? "Status: Running" : "Status: Stopped";
            ui::UI::TextDisabled(statusText.c_str());
            ui::UI::CheckboxRow("Collider Wireframes", "Draw collision shape overlays", &showWires);
            std::string wireStatus = showWires ? "Collider Debug: On" : "Collider Debug: Off";
            ui::UI::TextDisabled(wireStatus.c_str());
            bool groundChanged = ui::UI::CheckboxRow("Solid Ground", "Enable static ground plane", &solidGround);
            if (groundChanged) {
                if (jolt != nullptr) {
                    jolt->setGroundEnabled(solidGround);
                }
            }
            std::string groundStatus = solidGround ? "Ground Plane: On" : "Ground Plane: Off";
            ui::UI::TextDisabled(groundStatus.c_str());
            ui::UI::EndSurface();
            if (frameInfo.selectedEntity != entt::null) {
                auto        entity       = resolveSelectionForTransform(state_.scene(), frameInfo.selectedEntity);
                auto&       registry     = state_.scene().getRegistry();
                bool        hasRigidBody = registry.all_of<RigidBodyComponent>(entity);
                bool        hasCollider  = registry.all_of<ColliderComponent>(entity);
                std::string selectedText = "Selected: Object " + std::to_string((uint32_t) entity);
                ui::UI::BeginSurface("physics_entity", "Selected Entity", selectedText.c_str());
                if (!hasRigidBody) {
                    if (ui::UI::TonalButton((std::string(ICON_FA_PLUS) + " Add Rigid Body##physics_add_rb").c_str())) {
                        addPhysicsComponent(frameInfo);
                    }
                } else {
                    if (ui::UI::TonalButton((std::string(ICON_FA_MINUS) + " Remove Rigid Body##physics_remove_rb").c_str())) {
                        if (registry.all_of<RigidBodyComponent>(entity)) {
                            registry.remove<RigidBodyComponent>(entity);
                        }
                    }
                    editPhysicsProperties(frameInfo);
                }
                if (!hasCollider) {
                    if (ui::UI::TonalButton((std::string(ICON_FA_PLUS) + " Add Collider##physics_add_collider").c_str())) {
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
                        if (registry.all_of<RigidBodyComponent>(entity)) {
                            registry.get<RigidBodyComponent>(entity).pendingBodyStateOverride = true;
                        }
                    }
                } else {
                    if (ui::UI::TonalButton((std::string(ICON_FA_MINUS) + " Remove Collider##physics_remove_collider").c_str())) {
                        if (registry.all_of<ColliderComponent>(entity)) {
                            registry.remove<ColliderComponent>(entity);
                        }
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
                ui::UI::EndSurface();
            } else {
                ui::UI::BeginSurface("physics_empty", "Selected Entity", "No object selected");
                ui::UI::TextDisabled("Select an object in Scene to edit physics components.");
                ui::UI::EndSurface();
            }
        }
        ImGui::End();
        ui::UI::PopThemeStyle();
    }
    void PhysicsPanel::addPhysicsComponent(FrameInfo& frameInfo) {
        auto& registry = state_.scene().getRegistry();
        auto  entity   = frameInfo.selectedEntity;
        if (!registry.all_of<TransformComponent>(entity)) {
            registry.emplace<TransformComponent>(entity);
        }
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
        if (!registry.all_of<RigidBodyComponent>(entity)) {
            return;
        }
        auto& rb     = registry.get<RigidBodyComponent>(entity);
        bool  edited = false;
        ui::UI::SectionTitle("Rigid Body", "Mass and linear/angular dynamics");
        edited |= ui::UI::FloatRow("Mass", "Total body mass in kilograms", &rb.mass, 0.1f, 0.01f, 1000.0f);
        edited |= ui::UI::CheckboxRow("Static Body", "Locks the body in world space", &rb.isStatic);
        edited |= ui::UI::CheckboxRow("Use Gravity", "Apply gravity acceleration", &rb.useGravity);
        if (ui::UI::Section("Velocity")) {
            edited |= ui::UI::FloatRow("Velocity X", "Linear velocity axis X", &rb.velocity.x, 0.1f);
            edited |= ui::UI::FloatRow("Velocity Y", "Linear velocity axis Y", &rb.velocity.y, 0.1f);
            edited |= ui::UI::FloatRow("Velocity Z", "Linear velocity axis Z", &rb.velocity.z, 0.1f);
        }
        if (ui::UI::Section("Acceleration")) {
            edited |= ui::UI::FloatRow("Acceleration X", "Linear acceleration axis X", &rb.acceleration.x, 0.1f);
            edited |= ui::UI::FloatRow("Acceleration Y", "Linear acceleration axis Y", &rb.acceleration.y, 0.1f);
            edited |= ui::UI::FloatRow("Acceleration Z", "Linear acceleration axis Z", &rb.acceleration.z, 0.1f);
        }
        if (ui::UI::Section("Angular Velocity")) {
            edited |= ui::UI::FloatRow("Angular X", "Angular velocity axis X", &rb.angularVelocity.x, 0.01f);
            edited |= ui::UI::FloatRow("Angular Y", "Angular velocity axis Y", &rb.angularVelocity.y, 0.01f);
            edited |= ui::UI::FloatRow("Angular Z", "Angular velocity axis Z", &rb.angularVelocity.z, 0.01f);
        }
        if (edited) {
            rb.pendingBodyStateOverride = true;
        }
    }
    void PhysicsPanel::editColliderProperties(FrameInfo& frameInfo) {
        auto& registry = state_.scene().getRegistry();
        auto  entity   = frameInfo.selectedEntity;
        if (!registry.all_of<ColliderComponent>(entity)) {
            return;
        }
        auto&       col           = registry.get<ColliderComponent>(entity);
        auto*       rb            = registry.try_get<RigidBodyComponent>(entity);
        bool        changed       = false;
        const char* shapeLabels[] = {"Sphere", "Box", "Capsule", "Mesh"};
        int         si            = static_cast<int>(col.shape);
        ui::UI::SectionTitle("Collider", "Collision geometry and trigger behavior");
        if (ui::UI::EnumRow("Shape", "Collision primitive used for simulation", &si, shapeLabels, 4)) {
            col.shape = static_cast<ColliderComponent::ShapeType>(si);
            changed   = true;
        }
        changed |= ui::UI::CheckboxRow("Is Trigger", "Detect overlap without physical response", &col.isTrigger);
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
            if (rb != nullptr) {
                rb->pendingBodyStateOverride = true;
            }
        }
    }
}  // namespace engine