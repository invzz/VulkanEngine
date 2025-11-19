#include "TransformPanel.hpp"

#include <imgui.h>

#include <string>

namespace engine {

  TransformPanel::TransformPanel(GameObject::Map& gameObjects) : gameObjects_(gameObjects) {}

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

        // Lock axes checkbox
        ImGui::Checkbox("Lock Axes", &lockAxes_);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("When locked, all axes scale uniformly");
        }

        // Scale controls
        ImGui::Text("Scale:");

        // For animated objects, modify baseScale; for static objects, modify scale
        bool       isAnimated  = obj.animationController != nullptr;
        glm::vec3& targetScale = isAnimated ? obj.transform.baseScale : obj.transform.scale;

        if (isAnimated)
        {
          ImGui::TextDisabled("(Animated object - modifying base scale)");
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
