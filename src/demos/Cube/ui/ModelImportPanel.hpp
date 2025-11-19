#pragma once

#include <string>

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/systems/AnimationSystem.hpp"
#include "3dEngine/systems/PBRRenderSystem.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for importing glTF models
   */
  class ModelImportPanel : public UIPanel
  {
  public:
    ModelImportPanel(Device& device, GameObject::Map& gameObjects, AnimationSystem& animationSystem, PBRRenderSystem& pbrRenderSystem);

    void render(FrameInfo& frameInfo) override;

  private:
    Device&          device_;
    GameObject::Map& gameObjects_;
    AnimationSystem& animationSystem_;
    PBRRenderSystem& pbrRenderSystem_;
    char             modelPath_[256] = "";
  };

} // namespace engine
