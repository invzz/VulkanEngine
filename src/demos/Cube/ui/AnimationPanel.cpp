#include "AnimationPanel.hpp"

#include <imgui.h>

#include <string>

#include "Engine/Scene/components/AnimationComponent.hpp"

namespace engine {

  AnimationPanel::AnimationPanel(Scene& scene) : scene_(scene) {}

  void AnimationPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
    {
      auto view = scene_.getRegistry().view<AnimationComponent>();
      for (auto entity : view)
      {
        auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);

        std::string label = "Object " + std::to_string((uint32_t)entity);
        if (ImGui::TreeNode(label.c_str()))
        {
          bool isPlaying = anim.isPlaying;
          if (ImGui::Checkbox("Playing", &isPlaying))
          {
            if (isPlaying)
              anim.play(0, true);
            else
              anim.stop();
          }

          float speed = anim.playbackSpeed;
          if (ImGui::SliderFloat("Speed", &speed, 0.0f, 2.0f))
          {
            anim.playbackSpeed = speed;
          }

          ImGui::TreePop();
        }
      }
    }
  }

} // namespace engine
