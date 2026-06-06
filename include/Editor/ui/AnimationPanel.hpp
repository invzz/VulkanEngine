#ifndef EDITOR_ANIMATIONPANEL_HPP
#define EDITOR_ANIMATIONPANEL_HPP

#include "Engine/Scene/Scene.hpp"

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    /**
 * @brief Panel for animation controls
 */
    class AnimationPanel : public UIPanel {
       public:
        explicit AnimationPanel(Scene& scene);

        void render(FrameInfo& frameInfo) override;

       private:
        Scene& scene_;
    };

}  // namespace engine

#endif  // EDITOR_ANIMATIONPANEL_HPP
