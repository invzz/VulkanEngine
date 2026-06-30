#ifndef EDITOR_PHYSICSPANEL_HPP
#define EDITOR_PHYSICSPANEL_HPP

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class PhysicsPanel : public UIPanel {
       public:
        explicit PhysicsPanel(EngineState& state);

        void               render(FrameInfo& frameInfo) override;
        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }

       private:
        EngineState& state_;

        void addPhysicsComponent(FrameInfo& frameInfo);
        void editPhysicsProperties(FrameInfo& frameInfo);
        void editColliderProperties(FrameInfo& frameInfo);
    };

}  // namespace engine
#endif
