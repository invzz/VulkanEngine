#ifndef EDITOR_ATMOSPHERICPANEL_HPP
#define EDITOR_ATMOSPHERICPANEL_HPP

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    struct SkyboxSettings;

    /**
 * @brief Non-visible panel for atmospheric scattering parameters.
 *
 * Exposes beta (Rayleigh/Mie), scale heights, atmosphere radius,
 * mieG, and sun intensity for tweaking the procedural sky.
 * Hidden by default; toggled from the main toolbar.
 */
    class AtmosphericPanel : public UIPanel {
       public:
        explicit AtmosphericPanel(SkyboxSettings& settings);
        void render(FrameInfo& frameInfo) override;

       private:
        SkyboxSettings& settings_;

        void drawSkyOptions();
        void drawScatteringParams();
        void drawScaleHeights();
        void drawSunParams();
    };

}  // namespace engine

#endif
