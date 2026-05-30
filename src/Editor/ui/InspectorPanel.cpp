#include "Editor/ui/InspectorPanel.hpp"

#include <imgui.h>

#include <memory>

#include "Editor/ui/AnimationPanel.hpp"
#include "Editor/ui/LightsPanel.hpp"
#include "Editor/ui/TransformPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "entt/entity/entity.hpp"

namespace engine {

InspectorPanel::InspectorPanel(Scene& scene, bool* physicsSimulationRunning, bool* showColliderWireframes) {
  transformPanel_ = std::make_unique<TransformPanel>(scene);
  lightsPanel_ = std::make_unique<LightsPanel>(scene);
  animationPanel_ = std::make_unique<AnimationPanel>(scene);
  physicsPanel_ = std::make_unique<PhysicsPanel>(scene, physicsSimulationRunning, showColliderWireframes);
}

void InspectorPanel::render(FrameInfo& frameInfo) {
  if (!visible_) return;

  auto& registry = frameInfo.scene->getRegistry();
  if (frameInfo.selectedEntity != entt::null && !registry.valid(frameInfo.selectedEntity)) {
    frameInfo.selectedEntity = entt::null;
  }

  if (ImGui::Begin("Inspector", &visible_)) {
    if (frameInfo.selectedEntity != entt::null) {
      transformPanel_->render(frameInfo);
      ImGui::Separator();
      lightsPanel_->render(frameInfo);
      ImGui::Separator();
      animationPanel_->render(frameInfo);
    } else {
      ImGui::Text("No entity selected.");
    }

    ImGui::Separator();
    physicsPanel_->render(frameInfo);
  }
  ImGui::End();
}

}  // namespace engine
