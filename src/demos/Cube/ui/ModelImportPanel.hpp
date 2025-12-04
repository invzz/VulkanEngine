#pragma once

#include <string>

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/systems/AnimationSystem.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for importing glTF models
   */
  class ModelImportPanel : public UIPanel
  {
  public:
    ModelImportPanel(Device& device, GameObjectManager& objectManager, AnimationSystem& animationSystem);

    void render(FrameInfo& frameInfo) override;

  private:
    Device&            device_;
    GameObjectManager& objectManager_;
    AnimationSystem&   animationSystem_;
    char               modelPath_[256] = {};
  };

} // namespace engine
