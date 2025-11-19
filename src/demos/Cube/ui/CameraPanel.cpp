#include "CameraPanel.hpp"

#include <imgui.h>

namespace engine {

  CameraPanel::CameraPanel(GameObject& cameraObject) : cameraObject_(cameraObject) {}

  void CameraPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
      auto& pos = cameraObject_.transform.translation;
      ImGui::DragFloat3("Position", &pos.x, 0.01f);
    }
  }

} // namespace engine
