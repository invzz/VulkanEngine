#ifndef EDITOR_SCENEPANEL_HPP
#define EDITOR_SCENEPANEL_HPP

#include <vector>

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/Device.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneUtils.hpp"
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
    // Pending async model load data
    struct PendingModelLoad
    {
      std::future<std::shared_ptr<engine::Model>> future;
      std::string                                 path;
      std::string                                 name;
      engine::ModelInsertionOptions               options;
      bool                                        cancelled = false;
    };

    Device&                       device_;
    Scene&                        scene_;
    AnimationSystem&              animationSystem_;
    ResourceManager&              resourceManager_;
    std::vector<entt::entity>     toDelete_;
    std::vector<PendingModelLoad> pendingLoads_;
  };

} // namespace engine

#endif // EDITOR_SCENEPANEL_HPP
