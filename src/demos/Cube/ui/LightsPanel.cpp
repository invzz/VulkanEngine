#include "LightsPanel.hpp"

#include <imgui.h>

#include "Engine/Systems/LightSystem.hpp"

namespace engine {

  LightsPanel::LightsPanel(GameObjectManager& objectManager) : objectManager_(objectManager) {}

  void LightsPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (frameInfo.selectedObject)
      {
        auto& obj = *frameInfo.selectedObject;

        // Point Light Component
        if (auto* pointLight = obj.getComponent<PointLightComponent>())
        {
          ImGui::Text("Point Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##point", &pointLight->intensity, 0.1f, 0.0f, 100.0f);

          ImGui::Spacing();
        }

        // Directional Light Component
        if (auto* dirLight = obj.getComponent<DirectionalLightComponent>())
        {
          ImGui::Text("Directional Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##directional", &dirLight->intensity, 0.01f, 0.0f, 10.0f);

          ImGui::Spacing();
          ImGui::Text("Direction Control:");

          if (ImGui::Checkbox("Use Target Point##directional", &dirLight->useTargetPoint))
          {
            // When enabling, initialize target to a visible default if it's at origin
            if (dirLight->useTargetPoint)
            {
              if (glm::length(dirLight->targetPoint) < 0.01f)
              {
                dirLight->targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
              }
              LightSystem::updateTargetLockedLight(obj);
            }
          }

          if (dirLight->useTargetPoint)
          {
            if (ImGui::DragFloat3("Target Point", &dirLight->targetPoint.x, 0.1f))
            {
              LightSystem::updateTargetLockedLight(obj);
            }
          }

          // Show current direction
          glm::vec3 dir = obj.transform.getForwardDir();
          ImGui::Text("Current Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);

          ImGui::Spacing();
        }

        // Spot Light Component
        if (auto* spotLight = obj.getComponent<SpotLightComponent>())
        {
          ImGui::Text("Spot Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##spot", &spotLight->intensity, 0.1f, 0.0f, 100.0f);

          ImGui::Spacing();
          ImGui::Text("Direction Control:");

          if (ImGui::Checkbox("Use Target Point##spot", &spotLight->useTargetPoint))
          {
            // When enabling, initialize target to a visible default if it's at origin
            if (spotLight->useTargetPoint)
            {
              if (glm::length(spotLight->targetPoint) < 0.01f)
              {
                spotLight->targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
              }
              LightSystem::updateTargetLockedLight(obj);
            }
          }

          if (spotLight->useTargetPoint)
          {
            if (ImGui::DragFloat3("Target Point##spot", &spotLight->targetPoint.x, 0.1f))
            {
              LightSystem::updateTargetLockedLight(obj);
            }
          }

          // Show current direction
          glm::vec3 dir = obj.transform.getForwardDir();
          ImGui::Text("Current Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);

          ImGui::Spacing();
          ImGui::Text("Cone Angles:");
          ImGui::DragFloat("Inner Cutoff (deg)", &spotLight->innerCutoffAngle, 0.5f, 0.0f, 90.0f);
          ImGui::DragFloat("Outer Cutoff (deg)", &spotLight->outerCutoffAngle, 0.5f, 0.0f, 90.0f);

          // Ensure outer is always >= inner
          if (spotLight->outerCutoffAngle < spotLight->innerCutoffAngle)
          {
            spotLight->outerCutoffAngle = spotLight->innerCutoffAngle;
          }

          ImGui::Spacing();
          ImGui::Text("Attenuation:");
          ImGui::DragFloat("Constant", &spotLight->constantAttenuation, 0.01f, 0.0f, 10.0f);
          ImGui::DragFloat("Linear", &spotLight->linearAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");
          ImGui::DragFloat("Quadratic", &spotLight->quadraticAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");

          ImGui::Spacing();
        }

        // Color control (common for all lights)
        bool hasPointLight = obj.getComponent<PointLightComponent>() != nullptr;
        bool hasDirLight   = obj.getComponent<DirectionalLightComponent>() != nullptr;
        bool hasSpotLight  = obj.getComponent<SpotLightComponent>() != nullptr;

        if (hasPointLight || hasDirLight || hasSpotLight)
        {
          ImGui::Separator();
          ImGui::Text("Light Color:");
          ImGui::ColorEdit3("Color", &obj.color.x);
        }

        // Show message if no light component
        if (!hasPointLight && !hasDirLight && !hasSpotLight)
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
