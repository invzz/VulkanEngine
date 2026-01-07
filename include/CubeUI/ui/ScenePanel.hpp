#ifndef CUBE_UI_SCENEPANEL_HPP
#define CUBE_UI_SCENEPANEL_HPP

#include <vector>

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/AnimationSystem.hpp"

namespace engine {

  /**
   * @brief Panel for scene object management
   */
  class ScenePanel : public UIPanel
  {
  public:
    ScenePanel(Device& device, Scene& scene, AnimationSystem& animationSystem, ResourceManager& resourceManager);

    void               render(FrameInfo& frameInfo) override;
    [[nodiscard]] bool isSeparateWindow() const override { return true; }
    void               processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId);

  private:
    Device&                   device_;
    Scene&                    scene_;
    AnimationSystem&          animationSystem_;
    ResourceManager&          resourceManager_;
    std::vector<entt::entity> toDelete_;
  };

} // namespace engine

#endif // CUBE_UI_SCENEPANEL_HPP
