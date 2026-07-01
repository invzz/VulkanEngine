#ifndef EDITOR_INSPECTORPANEL_HPP
#define EDITOR_INSPECTORPANEL_HPP

#include <memory>

#include "Editor/ui/Panels/AnimationPanel.hpp"
#include "Editor/ui/Panels/LightsPanel.hpp"
#include "Editor/ui/Panels/TransformPanel.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class InspectorPanel : public UIPanel {
       public:
        explicit InspectorPanel(EngineState& state);

        void               render(FrameInfo& frameInfo) override;
        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }

       private:
        EngineState&                    state_;
        std::unique_ptr<TransformPanel> transformPanel_;
        std::unique_ptr<LightsPanel>    lightsPanel_;
        std::unique_ptr<AnimationPanel> animationPanel_;
    };

}  // namespace engine

#endif
