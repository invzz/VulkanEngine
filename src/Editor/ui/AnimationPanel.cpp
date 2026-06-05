#include "Editor/ui/AnimationPanel.hpp"

#include <imgui.h>

#include <cstdint>
#include <string>

#include "Editor/UI/UI.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"

namespace engine {

AnimationPanel::AnimationPanel(Scene& scene) : scene_(scene) {}

void AnimationPanel::render(FrameInfo& /*frameInfo*/) {
  if (!visible_) return;

  // Push theme style
  ui::UI::PushThemeStyle();

  // Use UI::Section for the collapsible header
  bool open = ui::UI::Section("Animation");
  if (!open) {
    ui::UI::PopThemeStyle();
    return;
  }

  auto view = scene_.getRegistry().view<AnimationComponent>();
  for (auto entity : view) {
    auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);

    std::string const label = "Object " + std::to_string((uint32_t)entity);
    if (ui::UI::TreeNode(label.c_str())) {
      bool isPlaying = anim.isPlaying;
      std::string checkboxLabel = "Playing##anim_" + std::to_string((uint32_t)entity);
      if (ui::UI::Checkbox(checkboxLabel.c_str(), &isPlaying)) {
        if (isPlaying) {
          anim.play(0, true);
        } else {
          anim.stop();
        }
      }

      float speed = anim.playbackSpeed;
      std::string speedLabel = "Speed##anim_" + std::to_string((uint32_t)entity);
      if (ui::UI::DragFloat(speedLabel.c_str(), &speed, 0.01f, 0.0f, 2.0f)) {
        anim.playbackSpeed = speed;
      }

      ImGui::TreePop();
    }
  }

  ui::UI::PopThemeStyle();
}

}  // namespace engine
