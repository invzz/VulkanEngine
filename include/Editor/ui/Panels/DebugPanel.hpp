#ifndef EDITOR_DEBUGPANEL_HPP
#define EDITOR_DEBUGPANEL_HPP
#include "Editor/ui/UIPanel.hpp"
namespace engine {
    class DebugPanel : public UIPanel {
       public:
        explicit DebugPanel(int& debugMode);
        void render(FrameInfo& frameInfo) override;

       private:
        int& debugMode_;
    };
}  // namespace engine
#endif
