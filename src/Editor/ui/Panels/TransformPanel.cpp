#include "Editor/ui/Panels/TransformPanel.hpp"

#include <imgui.h>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "entt/entity/entity.hpp"
#include "glm/trigonometric.hpp"

namespace engine {

    TransformPanel::TransformPanel(Scene& scene) : scene_(scene) {}

    void TransformPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        // Push theme style
        ui::UI::PushThemeStyle();

        // Use UI::Section for the collapsible header
        bool open = ui::UI::Section("Transform");
        if (!open) {
            ui::UI::PopThemeStyle();
            return;
        }

        if (frameInfo.selectedEntity != entt::null) {
            auto  entity   = frameInfo.selectedEntity;
            auto& registry = scene_.getRegistry();
            if (!registry.valid(entity) || !registry.all_of<TransformComponent>(entity)) {
                ui::UI::TextDisabled("Selected object has no transform");
                ui::UI::PopThemeStyle();
                return;
            }
            auto& transform = registry.get<TransformComponent>(entity);

            std::string entityStr = std::to_string((uint32_t) entity);
            ui::UI::TextColored(entityStr.c_str(), ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ui::UI::Separator();

            // Create tabs for Translation, Rotation, and Scale
            if (ImGui::BeginTabBar("TransformTabs")) {
                // Translation Tab
                if (ImGui::BeginTabItem("Translation")) {
                    ImGui::Spacing();
                    bool translationChanged = false;
                    translationChanged |= ui::UI::DragFloat("X##trans_x", &transform.translation.x, 0.1f);
                    translationChanged |= ui::UI::DragFloat("Y##trans_y", &transform.translation.y, 0.1f);
                    translationChanged |= ui::UI::DragFloat("Z##trans_z", &transform.translation.z, 0.1f);

                    ui::UI::Separator();
                    if (ui::UI::Button("Reset Position##trans_reset")) {
                        transform.translation = glm::vec3(0.0f);
                        translationChanged    = true;
                    }

                    if (translationChanged) {
                        if (auto* rigidBody = registry.try_get<RigidBodyComponent>(entity)) {
                            rigidBody->pendingBodyStateOverride = true;
                        }
                    }
                    ImGui::EndTabItem();
                }

                // Rotation Tab
                if (ImGui::BeginTabItem("Rotation")) {
                    ImGui::Spacing();
                    ui::UI::TextDisabled("Degrees:");
                    float rotationDegrees[3] = {
                        glm::degrees(transform.rotation.x),
                        glm::degrees(transform.rotation.y),
                        glm::degrees(transform.rotation.z)};
                    bool rotationChanged = false;

                    if (ui::UI::DragFloat("X##rot_x", &rotationDegrees[0], 1.0f, -180.0f, 180.0f)) {
                        transform.rotation.x = glm::radians(rotationDegrees[0]);
                        rotationChanged      = true;
                    }
                    if (ui::UI::DragFloat("Y##rot_y", &rotationDegrees[1], 1.0f, -180.0f, 180.0f)) {
                        transform.rotation.y = glm::radians(rotationDegrees[1]);
                        rotationChanged      = true;
                    }
                    if (ui::UI::DragFloat("Z##rot_z", &rotationDegrees[2], 1.0f, -180.0f, 180.0f)) {
                        transform.rotation.z = glm::radians(rotationDegrees[2]);
                        rotationChanged      = true;
                    }

                    ui::UI::Separator();
                    if (ui::UI::Button("Reset Rotation##rot_reset")) {
                        transform.rotation = glm::vec3(0.0f);
                        rotationChanged    = true;
                    }

                    if (rotationChanged) {
                        if (auto* rigidBody = registry.try_get<RigidBodyComponent>(entity)) {
                            rigidBody->pendingBodyStateOverride = true;
                        }
                    }
                    ImGui::EndTabItem();
                }

                // Scale Tab
                if (ImGui::BeginTabItem("Scale")) {
                    ImGui::Spacing();

                    // Lock axes checkbox
                    ui::UI::Checkbox("Lock Axes##scale_lock", &lockAxes_);
                    ui::UI::InfoTooltip("When locked, all axes scale uniformly");

                    ImGui::Spacing();

                    // For animated objects, modify baseScale; for static objects, modify scale
                    bool const isAnimated  = registry.all_of<AnimationComponent>(entity);
                    glm::vec3& targetScale = isAnimated ? transform.baseScale : transform.scale;

                    if (isAnimated) {
                        ui::UI::TextDisabled("(Animated - modifying base scale)");
                    }

                    if (lockAxes_) {
                        // Uniform scaling - use X axis as the master
                        float uniformScale = targetScale.x;
                        if (ui::UI::DragFloat("Uniform##scale_uniform", &uniformScale, 0.01f, 0.01f, 100.0f)) {
                            targetScale = glm::vec3(uniformScale);
                        }
                    } else {
                        // Independent axis scaling
                        ui::UI::DragFloat("X##scale_x", &targetScale.x, 0.01f, 0.01f, 100.0f);
                        ui::UI::DragFloat("Y##scale_y", &targetScale.y, 0.01f, 0.01f, 100.0f);
                        ui::UI::DragFloat("Z##scale_z", &targetScale.z, 0.01f, 0.01f, 100.0f);
                    }

                    // Quick scale buttons
                    ui::UI::Separator();
                    ui::UI::TextDisabled("Quick Scale:");
                    if (ui::UI::Button("Reset (1.0)##scale_reset")) {
                        targetScale = glm::vec3(1.0f);
                    }
                    ImGui::SameLine(0.0f, 2.0f);
                    if (ui::UI::Button("Half (0.5)##scale_half")) {
                        targetScale = glm::vec3(0.5f);
                    }
                    ImGui::SameLine(0.0f, 2.0f);
                    if (ui::UI::Button("Double (2.0)##scale_double")) {
                        targetScale = glm::vec3(2.0f);
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        } else {
            ui::UI::TextDisabled("No object selected");
            ui::UI::TextDisabled("Press Y/U to select objects");
            ui::UI::TextDisabled("Press C to select camera");
        }

        ui::UI::PopThemeStyle();
    }

}  // namespace engine
