#pragma once

#include "3dEngine/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for camera controls
   */
  class CameraPanel : public UIPanel
  {
  public:
    explicit CameraPanel(GameObject& cameraObject);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObject& cameraObject_;
  };

} // namespace engine
