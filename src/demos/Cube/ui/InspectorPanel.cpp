#include "InspectorPanel.hpp"

#include <imgui.h>

#include <memory>

#include "AnimationPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "LightsPanel.hpp"
#include "TransformPanel.hpp"
#include "entt/entity/entity.hpp"

namespace engine {

  InspectorPanel::InspectorPanel(Scene& scene)
  {
    transformPanel_ = std::make_unique<TransformPanel>(scene);
    lightsPanel_    = std::make_unique<LightsPanel>(scene);
    animationPanel_ = std::make_unique<AnimationPanel>(scene);
  }

  void InspectorPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::Begin("Inspector", &visible_))
    {
      if (frameInfo.selectedEntity != entt::null)
      {
        transformPanel_->render(frameInfo);
        lightsPanel_->render(frameInfo);
        animationPanel_->render(frameInfo);
      }
      else
      {
        ImGui::Text("No entity selected.");
      }
    }
    ImGui::End();
  }

} // namespace engine
