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

InspectorPanel::InspectorPanel(Scene& scene, bool* /*physicsSimulationRunning*/,
                               bool* /*showColliderWireframes*/, bool* /*solidGroundEnabled*/,
                               JoltPhysicsSystem* /*joltPhysicsSystem*/) {
  transformPanel_ = std::make_unique<TransformPanel>(scene);
  lightsPanel_    = std::make_unique<LightsPanel>(scene);
  animationPanel_ = std::make_unique<AnimationPanel>(scene);
  // PhysicsPanel removed — it is a separate dockable window, not nested here.
}

void InspectorPanel::render(FrameInfo& frameInfo) {
  if (!visible_) return;

  auto& registry = frameInfo.scene->getRegistry();
  if (frameInfo.selectedEntity != entt::null && !registry.valid(frameInfo.selectedEntity)) {
    frameInfo.selectedEntity = entt::null;
  }

  if (ImGui::Begin("Inspector", &visible_)) {
    if (frameInfo.selectedEntity != entt::null) {
      // Transform
      ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Transform");
      ImGui::Separator();
      transformPanel_->render(frameInfo);
      ImGui::Separator();

      // Lights
      ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Lights");
      ImGui::Separator();
      lightsPanel_->render(frameInfo);
      ImGui::Separator();

      // Animation
      ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "Animation");
      ImGui::Separator();
      animationPanel_->render(frameInfo);
    } else {
      ImGui::TextDisabled("No entity selected.");
      ImGui::TextDisabled("Select an entity in the Scene panel to inspect it.");
    }
  }
  ImGui::End();
}

}  // namespace engine
