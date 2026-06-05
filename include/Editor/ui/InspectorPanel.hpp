#ifndef EDITOR_INSPECTORPANEL_HPP
#define EDITOR_INSPECTORPANEL_HPP

#include <memory>

#include "Editor/ui/AnimationPanel.hpp"
#include "Editor/ui/LightsPanel.hpp"
#include "Editor/ui/TransformPanel.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

class Scene;
class FrameInfo;
class JoltPhysicsSystem;

class InspectorPanel : public UIPanel {
 public:
  InspectorPanel(Scene& scene, bool* physicsSimulationRunning, bool* showColliderWireframes,
                 bool* solidGroundEnabled, JoltPhysicsSystem* joltPhysicsSystem);

  void render(FrameInfo& frameInfo) override;
  [[nodiscard]] bool isSeparateWindow() const override {
    return true;
  }

 private:
  std::unique_ptr<TransformPanel> transformPanel_;
  std::unique_ptr<LightsPanel>    lightsPanel_;
  std::unique_ptr<AnimationPanel> animationPanel_;
};

}  // namespace engine

#endif  // EDITOR_INSPECTORPANEL_HPP
