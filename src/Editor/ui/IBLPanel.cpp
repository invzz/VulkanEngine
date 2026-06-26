#include "Editor/ui/IBLPanel.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Systems/IBLSystem.hpp"

#include "Editor/ui/UI.hpp"

namespace engine {

    IBLPanel::IBLPanel(EngineState* engineState) : engineState_(engineState) {
        if (engineState_ != nullptr) {
            auto rendering = engineState_->renderingService().view();
            if (rendering.iblSystem != nullptr) {
                settings_ = rendering.iblSystem->getSettings();
            }
        }
    }

    void IBLPanel::render(FrameInfo& /*frameInfo*/) {
        // Irradiance Size
        int currentIrradianceSizeIdx = 1;  // Default 64
        if (settings_.irradianceSize == 32)
            currentIrradianceSizeIdx = 0;
        if (settings_.irradianceSize == 64)
            currentIrradianceSizeIdx = 1;
        if (settings_.irradianceSize == 128)
            currentIrradianceSizeIdx = 2;
        if (settings_.irradianceSize == 256)
            currentIrradianceSizeIdx = 3;

        const char* sizeItems[] = {"32", "64", "128", "256"};
        if (ui::UI::Combo("Irradiance Size##ibl_irr_size", &currentIrradianceSizeIdx, sizeItems, IM_ARRAYSIZE(sizeItems))) {
            settings_.irradianceSize = std::stoi(sizeItems[currentIrradianceSizeIdx]);
        }

        // Prefilter Size
        int currentPrefilterSizeIdx = 2;  // Default 512
        if (settings_.prefilterSize == 128)
            currentPrefilterSizeIdx = 0;
        if (settings_.prefilterSize == 256)
            currentPrefilterSizeIdx = 1;
        if (settings_.prefilterSize == 512)
            currentPrefilterSizeIdx = 2;
        if (settings_.prefilterSize == 1024)
            currentPrefilterSizeIdx = 3;

        const char* prefilterSizeItems[] = {"128", "256", "512", "1024"};
        if (ui::UI::Combo("Prefilter Size##ibl_prefilter_size", &currentPrefilterSizeIdx, prefilterSizeItems, IM_ARRAYSIZE(prefilterSizeItems))) {
            settings_.prefilterSize = std::stoi(prefilterSizeItems[currentPrefilterSizeIdx]);
        }

        // Prefilter Mip Levels (int slider)
        ui::UI::SliderInt("Prefilter Mip Levels##ibl_prefilter_mip", &settings_.prefilterMipLevels, 1, 10);

        // Sample Count (int input)
        int sampleCount = static_cast<int>(
            std::min<uint32_t>(settings_.prefilterSampleCount, static_cast<uint32_t>(std::numeric_limits<int>::max())));
        if (ui::UI::InputInt("Prefilter Samples##ibl_prefilter_samples", &sampleCount)) {
            sampleCount                    = std::clamp(sampleCount, 1, std::numeric_limits<int>::max());
            settings_.prefilterSampleCount = sampleCount;
        }

        // Sample Delta (float)
        ui::UI::InputFloat("Irradiance Delta##ibl_irr_delta", &settings_.irradianceSampleDelta, 0.001f, 0.01f, "%.4f");

        if (ui::UI::Button("Regenerate IBL##ibl_regenerate")) {
            if (engineState_ != nullptr) {
                auto rendering  = engineState_->renderingService().view();
                auto sceneState = engineState_->sceneRuntimeService().view();
                if ((sceneState.skybox != nullptr) && (rendering.iblSystem != nullptr)) {
                    rendering.iblSystem->requestRegeneration(settings_, *sceneState.skybox);
                }
            }
        }

        bool hasSkybox = false;
        if (engineState_ != nullptr) {
            hasSkybox = (engineState_->sceneRuntimeService().view().skybox != nullptr);
        }

        if (!hasSkybox) {
            ui::UI::TextDisabled("No skybox loaded; IBL regeneration disabled");
            ui::UI::InfoTooltip("Load/enable a skybox to generate IBL");
        }
    }
}  // namespace engine
