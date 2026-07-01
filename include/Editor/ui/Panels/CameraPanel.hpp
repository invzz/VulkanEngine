#ifndef EDITOR_CAMERAPANEL_HPP
#define EDITOR_CAMERAPANEL_HPP

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class CameraPanel : public UIPanel {
       public:
        explicit CameraPanel(EngineState& state);

        void render(FrameInfo& frameInfo) override;

       private:
        EngineState& state_;
    };

}  // namespace engine
#endif
