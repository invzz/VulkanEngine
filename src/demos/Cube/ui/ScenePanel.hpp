#pragma once

#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for scene object management
   */
  class ScenePanel : public UIPanel
  {
  public:
    ScenePanel(Device& device, Scene& scene, AnimationSystem& animationSystem);

    void render(FrameInfo& frameInfo) override;
    bool isSeparateWindow() const override { return true; }
    void processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId);

  private:
    Device&                   device_;
    Scene&                    scene_;
    AnimationSystem&          animationSystem_;
    std::vector<entt::entity> toDelete_;
  };

} // namespace engine
