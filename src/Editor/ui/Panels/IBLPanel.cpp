#include "Editor/ui/Panels/IBLPanel.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

#include "Editor/ui/UI.hpp"

namespace engine {

    IBLPanel::IBLPanel(IBLSystem* ibl)
        : iblSystem_(ibl) {
        if (iblSystem_) {
            settings_ = iblSystem_->getSettings();
        }
    }

    void IBLPanel::render(FrameInfo& /*frameInfo*/) {
        int currentIrradianceSizeIdx = 1;
        if (settings_.irradianceSize == 32)
            currentIrradianceSizeIdx = 0;
        if (settings_.irradianceSize == 64)
            currentIrradianceSizeIdx = 1;
        if (settings_.irradianceSize == 128)
            currentIrradianceSizeIdx = 2;
        if (settings_.irradianceSize == 256)
            currentIrradianceSizeIdx = 3;

        const char* sizeItems[] = {"32", "64", "128", "256"};
        if (ui::UI::Combo("Irradiance Size##ibl_irr_size", &currentIrradianceSizeIdx, sizeItems, IM_ARRAYSIZE(sizeItems)))
            settings_.irradianceSize = std::stoi(sizeItems[currentIrradianceSizeIdx]);

        int currentPrefilterSizeIdx = 2;
        if (settings_.prefilterSize == 128)
            currentPrefilterSizeIdx = 0;
        if (settings_.prefilterSize == 256)
            currentPrefilterSizeIdx = 1;
        if (settings_.prefilterSize == 512)
            currentPrefilterSizeIdx = 2;
        if (settings_.prefilterSize == 1024)
            currentPrefilterSizeIdx = 3;

        const char* pItems[] = {"128", "256", "512", "1024"};
        if (ui::UI::Combo("Prefilter Size##ibl_prefilter_size", &currentPrefilterSizeIdx, pItems, IM_ARRAYSIZE(pItems)))
            settings_.prefilterSize = std::stoi(pItems[currentPrefilterSizeIdx]);

        ui::UI::SliderInt("Prefilter Mip Levels##ibl_prefilter_mip", &settings_.prefilterMipLevels, 1, 10);

        int sc = static_cast<int>(std::min<uint32_t>(settings_.prefilterSampleCount,
            static_cast<uint32_t>(std::numeric_limits<int>::max())));
        if (ui::UI::InputInt("Prefilter Samples##ibl_prefilter_samples", &sc)) {
            sc                             = std::clamp(sc, 1, std::numeric_limits<int>::max());
            settings_.prefilterSampleCount = sc;
        }

        ui::UI::InputFloat("Irradiance Delta##ibl_irr_delta", &settings_.irradianceSampleDelta, 0.001f, 0.01f, "%.4f");

        if (ui::UI::Button("Regenerate IBL##ibl_regenerate")) {
            // IBL regeneration requires skybox access — call via EngineState::syncEnvironmentLighting
        }

        if (iblSystem_ == nullptr)
            ui::UI::TextDisabled("No IBL system available");
    }
}  // namespace engine
