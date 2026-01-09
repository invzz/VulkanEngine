#ifndef CUBE_UI_CAMERAPANEL_HPP
#define CUBE_UI_CAMERAPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

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

#endif // CUBE_UI_CAMERAPANEL_HPP
