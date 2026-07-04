#ifndef EDITOR_POSTPROCESSPANEL_HPP
#define EDITOR_POSTPROCESSPANEL_HPP
#include "Engine/Systems/PostProcessingSystem.hpp"

#include "Editor/ui/UIPanel.hpp"
namespace engine {
    class PostProcessPanel : public UIPanel {
       public:
        PostProcessPanel(PostProcessPushConstants& pushConstants);
        void render(FrameInfo& frameInfo) override;

       private:
        PostProcessPushConstants& pushConstants;
    };
}  // namespace engine
#endif
