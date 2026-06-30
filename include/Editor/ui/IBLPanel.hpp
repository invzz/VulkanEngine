#ifndef EDITOR_IBLPANEL_HPP
#define EDITOR_IBLPANEL_HPP

#include "Engine/Systems/IBLSystem.hpp"

#include "Editor/ui/UIPanel.hpp"

namespace engine {
    class IBLPanel : public UIPanel {
       public:
        explicit IBLPanel(IBLSystem* ibl);
        void render(FrameInfo& frameInfo) override;

       private:
        IBLSystem*          iblSystem_ = nullptr;
        IBLSystem::Settings settings_;
    };
}  // namespace engine
#endif
