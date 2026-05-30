#ifndef EDITOR_SCENEPANEL_HPP
#define EDITOR_SCENEPANEL_HPP

#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/SceneUtils.hpp"

#include "Editor/ui/UIPanel.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    class EngineState;

    /**
 * @brief Panel for scene object management
 */
    class ScenePanel : public UIPanel {
       public:
        using StaticColliderImportMode = ModelInsertionOptions::StaticColliderImportMode;

        ScenePanel(Device& device, EngineState* engineState);

        void               render(FrameInfo& frameInfo) override;
        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }
        void processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId);

       private:
        struct PendingModelLoad {
            AsyncLoadId                   id{0};
            std::string                   path;
            std::string                   name;
            engine::ModelInsertionOptions options;
            StaticColliderImportMode      colliderMode{StaticColliderImportMode::AutoDetect};
            bool                          cancelled = false;
        };

            Device&                   device_;
            EngineState*              engineState_ = nullptr;
            std::vector<entt::entity> toDelete_;
        std::vector<PendingModelLoad> pendingLoads_;
            StaticColliderImportMode colliderImportMode_{StaticColliderImportMode::AutoDetect};
    };

}  // namespace engine

#endif  // EDITOR_SCENEPANEL_HPP
