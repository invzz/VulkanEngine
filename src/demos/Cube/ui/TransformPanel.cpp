#include "TransformPanel.hpp"

#include <imgui.h>

#include <string>

#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"

namespace engine {

  TransformPanel::TransformPanel(Scene& scene) : scene_(scene) {}

  void TransformPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (frameInfo.selectedEntity != entt::null)
      {
        auto  entity    = frameInfo.selectedEntity;
        auto& registry  = scene_.getRegistry();
        auto& transform = registry.get<TransformComponent>(entity);

        ImGui::Text("Selected: Object %u", (uint32_t)entity);
        ImGui::Separator();

        // Create tabs for Translation, Rotation, and Scale
        if (ImGui::BeginTabBar("TransformTabs"))
        {
          // Translation Tab
          if (ImGui::BeginTabItem("Translation"))
          {
            ImGui::Spacing();
            bool translationChanged = false;
            translationChanged |= ImGui::DragFloat("X", &transform.translation.x, 0.1f);
            translationChanged |= ImGui::DragFloat("Y", &transform.translation.y, 0.1f);
            translationChanged |= ImGui::DragFloat("Z", &transform.translation.z, 0.1f);

            // If translation changed and light is target-locked, update rotation
            if (translationChanged)
            {
              bool isDirLocked = false;
              if (registry.all_of<DirectionalLightComponent>(entity))
              {
                isDirLocked = registry.get<DirectionalLightComponent>(entity).useTargetPoint;
              }

              bool isSpotLocked = false;
              if (registry.all_of<SpotLightComponent>(entity))
              {
                isSpotLocked = registry.get<SpotLightComponent>(entity).useTargetPoint;
              }

              if (isDirLocked || isSpotLocked)
              {
                LightSystem::updateTargetLockedLight(entity, &scene_);
              }
            }

            ImGui::Separator();
            if (ImGui::Button("Reset Position"))
            {
              transform.translation = glm::vec3(0.0f);
              // Update rotation if target-locked
              bool isDirLocked = false;
              if (registry.all_of<DirectionalLightComponent>(entity))
              {
                isDirLocked = registry.get<DirectionalLightComponent>(entity).useTargetPoint;
              }

              bool isSpotLocked = false;
              if (registry.all_of<SpotLightComponent>(entity))
              {
                isSpotLocked = registry.get<SpotLightComponent>(entity).useTargetPoint;
              }

              if (isDirLocked || isSpotLocked)
              {
                LightSystem::updateTargetLockedLight(entity, &scene_);
              }
            }
            ImGui::EndTabItem();
          }

          // Rotation Tab
          if (ImGui::BeginTabItem("Rotation"))
          {
            ImGui::Spacing();
            ImGui::Text("Degrees:");
            float rotationDegrees[3] = {glm::degrees(transform.rotation.x), glm::degrees(transform.rotation.y), glm::degrees(transform.rotation.z)};

            if (ImGui::DragFloat("X", &rotationDegrees[0], 1.0f, -180.0f, 180.0f))
            {
              transform.rotation.x = glm::radians(rotationDegrees[0]);
            }
            if (ImGui::DragFloat("Y", &rotationDegrees[1], 1.0f, -180.0f, 180.0f))
            {
              transform.rotation.y = glm::radians(rotationDegrees[1]);
            }
            if (ImGui::DragFloat("Z", &rotationDegrees[2], 1.0f, -180.0f, 180.0f))
            {
              transform.rotation.z = glm::radians(rotationDegrees[2]);
            }

            ImGui::Separator();
            if (ImGui::Button("Reset Rotation"))
            {
              transform.rotation = glm::vec3(0.0f);
            }
            ImGui::EndTabItem();
          }

          // Scale Tab
          if (ImGui::BeginTabItem("Scale"))
          {
            ImGui::Spacing();

            // Lock axes checkbox
            ImGui::Checkbox("Lock Axes", &lockAxes_);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
              ImGui::SetTooltip("When locked, all axes scale uniformly");
            }

            ImGui::Spacing();

            // For animated objects, modify baseScale; for static objects, modify scale
            bool       isAnimated  = registry.all_of<AnimationComponent>(entity);
            glm::vec3& targetScale = isAnimated ? transform.baseScale : transform.scale;

            if (isAnimated)
            {
              ImGui::TextDisabled("(Animated - modifying base scale)");
            }

            if (lockAxes_)
            {
              // Uniform scaling - use X axis as the master
              float uniformScale = targetScale.x;
              if (ImGui::DragFloat("Uniform", &uniformScale, 0.01f, 0.01f, 100.0f))
              {
                targetScale = glm::vec3(uniformScale);
              }
            }
            else
            {
              // Independent axis scaling
              ImGui::DragFloat("X", &targetScale.x, 0.01f, 0.01f, 100.0f);
              ImGui::DragFloat("Y", &targetScale.y, 0.01f, 0.01f, 100.0f);
              ImGui::DragFloat("Z", &targetScale.z, 0.01f, 0.01f, 100.0f);
            }

            // Quick scale buttons
            ImGui::Separator();
            ImGui::Text("Quick Scale:");
            if (ImGui::Button("Reset (1.0)"))
            {
              targetScale = glm::vec3(1.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Half (0.5)"))
            {
              targetScale = glm::vec3(0.5f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Double (2.0)"))
            {
              targetScale = glm::vec3(2.0f);
            }
            ImGui::EndTabItem();
          }

          ImGui::EndTabBar();
        }
      }
      else
      {
        ImGui::TextDisabled("No object selected");
        ImGui::TextDisabled("Press Y/U to select objects");
        ImGui::TextDisabled("Press C to select camera");
      }
    }
  }

} // namespace engine
