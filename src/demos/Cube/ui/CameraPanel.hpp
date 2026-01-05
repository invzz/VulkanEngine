#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_CAMERAPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_CAMERAPANEL_HPP

#include "Engine/Scene/Scene.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for camera controls
   */
  class CameraPanel : public UIPanel
  {
  public:
    explicit CameraPanel(entt::entity cameraEntity, Scene* scene);

    void render(FrameInfo& frameInfo) override;

  private:
    entt::entity cameraEntity_;
    Scene*       scene_;
  };

} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_CAMERAPANEL_HPP
