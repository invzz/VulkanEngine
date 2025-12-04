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
        if (obj.getComponent<AnimationController>())
        {
          std::string label = "Object " + std::to_string(id);
          if (ImGui::TreeNode(label.c_str()))
          {
            bool isPlaying = obj.getComponent<AnimationController>()->isPlaying();
            if (ImGui::Checkbox("Playing", &isPlaying))
            {
              if (isPlaying)
                obj.getComponent<AnimationController>()->play(0, true);
              else
                obj.getComponent<AnimationController>()->stop();
            }

            float speed = obj.getComponent<AnimationController>()->getPlaybackSpeed();
            if (ImGui::SliderFloat("Speed", &speed, 0.0f, 2.0f))
            {
              obj.getComponent<AnimationController>()->setPlaybackSpeed(speed);
            }

            ImGui::TreePop();
          }
        }
      }
    }
  }

} // namespace engine
