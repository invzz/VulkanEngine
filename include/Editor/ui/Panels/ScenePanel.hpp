#ifndef EDITOR_SCENEPANEL_HPP
#define EDITOR_SCENEPANEL_HPP

#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/SceneUtils.hpp"

#include "Editor/ui/UI.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class ScenePanel : public UIPanel {
       public:
        using StaticColliderImportMode = ModelInsertionOptions::StaticColliderImportMode;

        ScenePanel(Device& device, EngineState& state);

        void               render(FrameInfo& frameInfo) override;
        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }
        void processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId);

       private:
        Device&                                device_;
        EngineState&                           state_;
        std::vector<entt::entity>              toDelete_;
        std::vector<ui::ScenePendingModelLoad> pendingLoads_;
        StaticColliderImportMode               colliderImportMode_{StaticColliderImportMode::AutoDetect};
    };

}  // namespace engine
#endif
