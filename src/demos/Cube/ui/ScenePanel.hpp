#pragma once

#include <vector>

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/systems/AnimationSystem.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for scene object management
   */
  class ScenePanel : public UIPanel
  {
  public:
    ScenePanel(Device& device, GameObjectManager& objectManager, AnimationSystem& animationSystem);

    void render(FrameInfo& frameInfo) override;
    void processDelayedDeletions(); // Call this between frames, not during rendering

  private:
    struct PendingDeletion
    {
      GameObject::id_t id;
      int              framesRemaining;
    };

    Device&                       device_;
    GameObjectManager&            objectManager_;
    AnimationSystem&              animationSystem_;
    std::vector<GameObject::id_t> toDelete_;
    std::vector<PendingDeletion>  pendingDeletions_;
    static constexpr int          FRAMES_TO_WAIT = 3; // Wait a few frames before deletion
  };

} // namespace engine
