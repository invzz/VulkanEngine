#include "AnimationPanel.hpp"

#include <imgui.h>

#include <string>

namespace engine {

  AnimationPanel::AnimationPanel(GameObjectManager& objectManager) : objectManager_(objectManager) {}

  void AnimationPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
    {
      for (auto& [id, obj] : objectManager_.getAllObjects())
      {
        if (obj.animationController)
        {
          std::string label = "Object " + std::to_string(id);
          if (ImGui::TreeNode(label.c_str()))
          {
            bool isPlaying = obj.animationController->isPlaying();
            if (ImGui::Checkbox("Playing", &isPlaying))
            {
              if (isPlaying)
                obj.animationController->play(0, true);
              else
                obj.animationController->stop();
            }

            float speed = obj.animationController->getPlaybackSpeed();
            if (ImGui::SliderFloat("Speed", &speed, 0.0f, 2.0f))
            {
              obj.animationController->setPlaybackSpeed(speed);
            }

            ImGui::TreePop();
          }
        }
      }
    }
  }

} // namespace engine
