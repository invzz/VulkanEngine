#ifndef EDITOR_IBLPANEL_HPP
#define EDITOR_IBLPANEL_HPP

#include "Engine/Systems/IBLSystem.hpp"

#include "Editor/ui/UIPanel.hpp"

namespace engine {
    class EngineState;
    class IBLPanel : public UIPanel {
       public:
        explicit IBLPanel(EngineState* engineState);
        void render(FrameInfo& frameInfo) override;

       private:
        EngineState*        engineState_ = nullptr;
        IBLSystem::Settings settings_;
    };
}  // namespace engine

#endif  // EDITOR_IBLPANEL_HPP
