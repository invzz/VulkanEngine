#include "Editor/ui/CameraPanel.hpp"

#include <imgui.h>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "entt/entity/fwd.hpp"

namespace engine {

CameraPanel::CameraPanel(entt::entity cameraEntity, Scene* scene) : cameraEntity_(cameraEntity), scene_(scene) {}

void CameraPanel::render(FrameInfo& frameInfo) {
  entt::entity currentCamera = cameraEntity_;
  if (scene_ != nullptr) {
    // Prefer the actively selected camera from frame state; fallback to cached one.
    if (scene_->getRegistry().valid(frameInfo.cameraEntity)) {
      currentCamera = frameInfo.cameraEntity;
      cameraEntity_ = frameInfo.cameraEntity;
    }
  }

  if (scene_->getRegistry().valid(currentCamera)) {
    if (scene_->getRegistry().all_of<TransformComponent>(currentCamera)) {
      auto& pos = scene_->getRegistry().get<TransformComponent>(currentCamera).translation;
      ImGui::DragFloat3("Position", &pos.x, 0.01f);
    }

    if (scene_->getRegistry().all_of<CameraComponent>(currentCamera)) {
      auto& camComp = scene_->getRegistry().get<CameraComponent>(currentCamera);

      bool isOrtho = camComp.isOrthographic;
      if (ImGui::Checkbox("Orthographic", &isOrtho)) {
        camComp.isOrthographic = isOrtho;
      }

      if (isOrtho) {
        ImGui::DragFloat("Ortho Size", &camComp.orthoSize, 0.1f, 0.1f, 100.0f);
      } else {
        ImGui::DragFloat("FOV", &camComp.fovY, 0.1f, 1.0f, 179.0f);
      }

      ImGui::DragFloat("Near Plane", &camComp.nearZ, 0.01f, 0.001f, 10.0f);
      ImGui::DragFloat("Far Plane", &camComp.farZ, 1.0f, 10.0f, 10000.0f);
    }
  }
}

}  // namespace engine
