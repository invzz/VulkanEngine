#include "TransformPanel.hpp"

#include <imgui.h>

#include <string>

#include "Engine/Systems/LightSystem.hpp"

namespace engine {

  TransformPanel::TransformPanel(GameObjectManager& objectManager) : objectManager_(objectManager) {}

  void TransformPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (frameInfo.selectedObject)
      {
        auto& obj = *frameInfo.selectedObject;
        ImGui::Text("Selected: Object %u", frameInfo.selectedObjectId);
        ImGui::Separator();

        // Create tabs for Translation, Rotation, and Scale
        if (ImGui::BeginTabBar("TransformTabs"))
        {
          // Translation Tab
          if (ImGui::BeginTabItem("Translation"))
          {
            ImGui::Spacing();
            bool translationChanged = false;
            translationChanged |= ImGui::DragFloat("X", &obj.transform.translation.x, 0.1f);
            translationChanged |= ImGui::DragFloat("Y", &obj.transform.translation.y, 0.1f);
            translationChanged |= ImGui::DragFloat("Z", &obj.transform.translation.z, 0.1f);

            // If translation changed and light is target-locked, update rotation
            if (translationChanged)
            {
              bool isDirLocked  = obj.getComponent<DirectionalLightComponent>() && obj.getComponent<DirectionalLightComponent>()->useTargetPoint;
              bool isSpotLocked = obj.getComponent<SpotLightComponent>() && obj.getComponent<SpotLightComponent>()->useTargetPoint;

              if (isDirLocked || isSpotLocked)
              {
                LightSystem::updateTargetLockedLight(obj);
              }
            }

            ImGui::Separator();
            if (ImGui::Button("Reset Position"))
            {
              obj.transform.translation = glm::vec3(0.0f);
              // Update rotation if target-locked
              bool isDirLocked  = obj.getComponent<DirectionalLightComponent>() && obj.getComponent<DirectionalLightComponent>()->useTargetPoint;
              bool isSpotLocked = obj.getComponent<SpotLightComponent>() && obj.getComponent<SpotLightComponent>()->useTargetPoint;

              if (isDirLocked || isSpotLocked)
              {
                LightSystem::updateTargetLockedLight(obj);
              }
            }
            ImGui::EndTabItem();
          }

          // Rotation Tab
          if (ImGui::BeginTabItem("Rotation"))
          {
            ImGui::Spacing();
            ImGui::Text("Degrees:");
            float rotationDegrees[3] = {glm::degrees(obj.transform.rotation.x), glm::degrees(obj.transform.rotation.y), glm::degrees(obj.transform.rotation.z)};

            if (ImGui::DragFloat("X", &rotationDegrees[0], 1.0f, -180.0f, 180.0f))
            {
              obj.transform.rotation.x = glm::radians(rotationDegrees[0]);
            }
            if (ImGui::DragFloat("Y", &rotationDegrees[1], 1.0f, -180.0f, 180.0f))
            {
              obj.transform.rotation.y = glm::radians(rotationDegrees[1]);
            }
            if (ImGui::DragFloat("Z", &rotationDegrees[2], 1.0f, -180.0f, 180.0f))
            {
              obj.transform.rotation.z = glm::radians(rotationDegrees[2]);
            }

            ImGui::Separator();
            if (ImGui::Button("Reset Rotation"))
            {
              obj.transform.rotation = glm::vec3(0.0f);
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
            bool       isAnimated  = obj.getComponent<AnimationController>() != nullptr;
            glm::vec3& targetScale = isAnimated ? obj.transform.baseScale : obj.transform.scale;

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
