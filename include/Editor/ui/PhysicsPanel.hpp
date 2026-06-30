#ifndef EDITOR_PHYSICSPANEL_HPP
#define EDITOR_PHYSICSPANEL_HPP

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class PhysicsPanel : public UIPanel {
       public:
        explicit PhysicsPanel(EngineState& state);

        void render(FrameInfo& frameInfo) override;
        // PhysicsPanel docks into the main dockspace like all other panels.
        // The dock zone is configured via DockConstraints in app.cpp.

       private:
        EngineState& state_;

        void addPhysicsComponent(FrameInfo& frameInfo);
        void editPhysicsProperties(FrameInfo& frameInfo);
        void editColliderProperties(FrameInfo& frameInfo);
    };

}  // namespace engine
#endif
