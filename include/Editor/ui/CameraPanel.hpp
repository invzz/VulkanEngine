#ifndef EDITOR_CAMERAPANEL_HPP
#define EDITOR_CAMERAPANEL_HPP

#include "Editor/ui/UIPanel.hpp"
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

#endif // EDITOR_CAMERAPANEL_HPP
