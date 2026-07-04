#ifndef EDITOR_TRANSFORMPANEL_HPP
#define EDITOR_TRANSFORMPANEL_HPP
#include "Engine/Scene/Scene.hpp"

#include "Editor/ui/UIPanel.hpp"
namespace engine {
    class TransformPanel : public UIPanel {
       public:
        TransformPanel(Scene& scene);
        void render(FrameInfo& frameInfo) override;

       private:
        Scene& scene_;
        bool   lockAxes_ = false;
    };
}  // namespace engine
#endif
