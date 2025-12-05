#include "LightsPanel.hpp"

#include <imgui.h>

#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"

namespace engine {

  LightsPanel::LightsPanel(Scene& scene) : scene_(scene) {}

  void LightsPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (frameInfo.selectedEntity != entt::null)
      {
        auto  entity   = frameInfo.selectedEntity;
        auto& registry = scene_.getRegistry();

        // Point Light Component
        if (registry.all_of<PointLightComponent>(entity))
        {
          auto& pointLight = registry.get<PointLightComponent>(entity);
          ImGui::Text("Point Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##point", &pointLight.intensity, 0.1f, 0.0f, 100.0f);
          ImGui::ColorEdit3("Color##point", &pointLight.color.x);
          ImGui::DragFloat("Radius##point", &pointLight.radius, 0.01f, 0.0f, 100.0f);

          ImGui::Spacing();
        }

        // Directional Light Component
        if (registry.all_of<DirectionalLightComponent>(entity))
        {
          auto& dirLight = registry.get<DirectionalLightComponent>(entity);
          ImGui::Text("Directional Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##directional", &dirLight.intensity, 0.01f, 0.0f, 10.0f);
          ImGui::ColorEdit3("Color##directional", &dirLight.color.x);

          ImGui::Spacing();
          ImGui::Text("Direction Control:");

          if (ImGui::Checkbox("Use Target Point##directional", &dirLight.useTargetPoint))
          {
            // When enabling, initialize target to a visible default if it's at origin
            if (dirLight.useTargetPoint)
            {
              if (glm::length(dirLight.targetPoint) < 0.01f)
              {
                dirLight.targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
              }
              LightSystem::updateTargetLockedLight(entity, &scene_);
            }
          }

          if (dirLight.useTargetPoint)
          {
            if (ImGui::DragFloat3("Target Point", &dirLight.targetPoint.x, 0.1f))
            {
              LightSystem::updateTargetLockedLight(entity, &scene_);
            }
          }

          // Show current direction
          glm::vec3 dir = registry.get<TransformComponent>(entity).getForwardDir();
          ImGui::Text("Current Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);

          ImGui::Spacing();
        }

        // Spot Light Component
        if (registry.all_of<SpotLightComponent>(entity))
        {
          auto& spotLight = registry.get<SpotLightComponent>(entity);
          ImGui::Text("Spot Light");
          ImGui::Separator();

          ImGui::DragFloat("Intensity##spot", &spotLight.intensity, 0.1f, 0.0f, 100.0f);
          ImGui::ColorEdit3("Color##spot", &spotLight.color.x);

          ImGui::Spacing();
          ImGui::Text("Direction Control:");

          if (ImGui::Checkbox("Use Target Point##spot", &spotLight.useTargetPoint))
          {
            // When enabling, initialize target to a visible default if it's at origin
            if (spotLight.useTargetPoint)
            {
              if (glm::length(spotLight.targetPoint) < 0.01f)
              {
                spotLight.targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
              }
              LightSystem::updateTargetLockedLight(entity, &scene_);
            }
          }

          if (spotLight.useTargetPoint)
          {
            if (ImGui::DragFloat3("Target Point##spot", &spotLight.targetPoint.x, 0.1f))
            {
              LightSystem::updateTargetLockedLight(entity, &scene_);
            }
          }

          // Show current direction
          glm::vec3 dir = registry.get<TransformComponent>(entity).getForwardDir();
          ImGui::Text("Current Dir: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);

          ImGui::Spacing();
          ImGui::Text("Cone Angles:");
          ImGui::DragFloat("Inner Cutoff (deg)", &spotLight.innerCutoffAngle, 0.5f, 0.0f, 90.0f);
          ImGui::DragFloat("Outer Cutoff (deg)", &spotLight.outerCutoffAngle, 0.5f, 0.0f, 90.0f);

          // Ensure outer is always >= inner
          if (spotLight.outerCutoffAngle < spotLight.innerCutoffAngle)
          {
            spotLight.outerCutoffAngle = spotLight.innerCutoffAngle;
          }

          ImGui::Spacing();
          ImGui::Text("Attenuation:");
          ImGui::DragFloat("Constant", &spotLight.constantAttenuation, 0.01f, 0.0f, 10.0f);
          ImGui::DragFloat("Linear", &spotLight.linearAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");
          ImGui::DragFloat("Quadratic", &spotLight.quadraticAttenuation, 0.001f, 0.0f, 1.0f, "%.4f");
        }

        // Show message if no light component
        bool hasPointLight = registry.all_of<PointLightComponent>(entity);
        bool hasDirLight   = registry.all_of<DirectionalLightComponent>(entity);
        bool hasSpotLight  = registry.all_of<SpotLightComponent>(entity);

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
