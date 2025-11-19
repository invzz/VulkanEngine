#include "LightsPanel.hpp"

#include <imgui.h>

namespace engine {

  LightsPanel::LightsPanel(GameObject::Map& gameObjects) : gameObjects_(gameObjects) {}

  void LightsPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (frameInfo.selectedObject)
      {
        auto& obj = *frameInfo.selectedObject;

        // Point Light Component
        if (obj.pointLight)
        {
          ImGui::Text("Point Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##point", &obj.pointLight->intensity, 0.1f, 0.0f, 100.0f);

          ImGui::Spacing();
        }

        // Directional Light Component
        if (obj.directionalLight)
        {
          ImGui::Text("Directional Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##directional", &obj.directionalLight->intensity, 0.01f, 0.0f, 10.0f);

          ImGui::Spacing();
        }

        // Spot Light Component
        if (obj.spotLight)
        {
          ImGui::Text("Spot Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##spot", &obj.spotLight->intensity, 0.1f, 0.0f, 100.0f);

          ImGui::Text("Cone Angles:");
          ImGui::DragFloat("Inner Cutoff (deg)", &obj.spotLight->innerCutoffAngle, 0.5f, 0.0f, 90.0f);
          ImGui::DragFloat("Outer Cutoff (deg)", &obj.spotLight->outerCutoffAngle, 0.5f, 0.0f, 90.0f);

          // Ensure outer is always >= inner
          if (obj.spotLight->outerCutoffAngle < obj.spotLight->innerCutoffAngle)
          {
            obj.spotLight->outerCutoffAngle = obj.spotLight->innerCutoffAngle;
          }

          ImGui::Spacing();
          ImGui::Text("Attenuation:");
          ImGui::DragFloat("Constant", &obj.spotLight->constantAttenuation, 0.01f, 0.0f, 10.0f);
          ImGui::DragFloat("Linear", &obj.spotLight->linearAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");
          ImGui::DragFloat("Quadratic", &obj.spotLight->quadraticAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");

          ImGui::Spacing();
        }

        // Color control (common for all lights)
        if (obj.pointLight || obj.directionalLight || obj.spotLight)
        {
          ImGui::Separator();
          ImGui::Text("Light Color:");
          ImGui::ColorEdit3("Color", &obj.color.x);
        }

        // Show message if no light component
        if (!obj.pointLight && !obj.directionalLight && !obj.spotLight)
        {
          ImGui::TextDisabled("No light component");
          ImGui::TextDisabled("This object is not a light");
        }
      }
      else
      {
        ImGui::TextDisabled("No object selected");
        ImGui::TextDisabled("Press Y/U to select objects");
      }
    }
  }

} // namespace engine
