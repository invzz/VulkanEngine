#include "CameraPanel.hpp"

#include <imgui.h>

#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  CameraPanel::CameraPanel(entt::entity cameraEntity, Scene* scene) : cameraEntity_(cameraEntity), scene_(scene) {}

  void CameraPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
      if (scene_->getRegistry().valid(cameraEntity_))
      {
        if (scene_->getRegistry().all_of<TransformComponent>(cameraEntity_))
        {
          auto& pos = scene_->getRegistry().get<TransformComponent>(cameraEntity_).translation;
          ImGui::DragFloat3("Position", &pos.x, 0.01f);
        }

        if (scene_->getRegistry().all_of<CameraComponent>(cameraEntity_))
        {
          auto& camComp = scene_->getRegistry().get<CameraComponent>(cameraEntity_);

          bool isOrtho = camComp.isOrthographic;
          if (ImGui::Checkbox("Orthographic", &isOrtho))
          {
            camComp.isOrthographic = isOrtho;
          }

          if (isOrtho)
          {
            ImGui::DragFloat("Ortho Size", &camComp.orthoSize, 0.1f, 0.1f, 100.0f);
          }
          else
          {
            ImGui::DragFloat("FOV", &camComp.fovY, 0.1f, 1.0f, 179.0f);
          }

          ImGui::DragFloat("Near Plane", &camComp.nearZ, 0.01f, 0.001f, 10.0f);
          ImGui::DragFloat("Far Plane", &camComp.farZ, 1.0f, 10.0f, 10000.0f);
        }
      }
    }
  }

} // namespace engine
