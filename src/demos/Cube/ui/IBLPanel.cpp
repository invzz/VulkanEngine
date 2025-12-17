#include "IBLPanel.hpp"

#include <imgui.h>

#include <string>

namespace engine {

  IBLPanel::IBLPanel(IBLSystem& iblSystem, Skybox& skybox) : iblSystem_{iblSystem}, skybox_{skybox}
  {
    settings_ = iblSystem_.getSettings();
  }

  void IBLPanel::render(FrameInfo& frameInfo)
  {
    // ImGui::Begin("IBL Settings");

    bool changed = false;

    // Irradiance Size
    int currentIrradianceSizeIdx = 1; // Default 64
    if (settings_.irradianceSize == 32) currentIrradianceSizeIdx = 0;
    if (settings_.irradianceSize == 64) currentIrradianceSizeIdx = 1;
    if (settings_.irradianceSize == 128) currentIrradianceSizeIdx = 2;
    if (settings_.irradianceSize == 256) currentIrradianceSizeIdx = 3;

    const char* sizeItems[] = {"32", "64", "128", "256"};
    if (ImGui::Combo("Irradiance Size", &currentIrradianceSizeIdx, sizeItems, IM_ARRAYSIZE(sizeItems)))
    {
      settings_.irradianceSize = std::stoi(sizeItems[currentIrradianceSizeIdx]);
      changed                  = true;
    }

    // Prefilter Size
    int currentPrefilterSizeIdx = 2; // Default 512
    if (settings_.prefilterSize == 128) currentPrefilterSizeIdx = 0;
    if (settings_.prefilterSize == 256) currentPrefilterSizeIdx = 1;
    if (settings_.prefilterSize == 512) currentPrefilterSizeIdx = 2;
    if (settings_.prefilterSize == 1024) currentPrefilterSizeIdx = 3;

    const char* prefilterSizeItems[] = {"128", "256", "512", "1024"};
    if (ImGui::Combo("Prefilter Size", &currentPrefilterSizeIdx, prefilterSizeItems, IM_ARRAYSIZE(prefilterSizeItems)))
    {
      settings_.prefilterSize = std::stoi(prefilterSizeItems[currentPrefilterSizeIdx]);
      changed                 = true;
    }

    // Prefilter Mip Levels
    if (ImGui::SliderInt("Prefilter Mip Levels", &settings_.prefilterMipLevels, 1, 10)) changed = true;

    // Sample Count
    int sampleCount = static_cast<int>(settings_.prefilterSampleCount);
    if (ImGui::InputInt("Prefilter Samples", &sampleCount))
    {
      settings_.prefilterSampleCount = static_cast<uint32_t>(sampleCount);
      changed                        = true;
    }

    // Sample Delta
    if (ImGui::InputFloat("Irradiance Delta", &settings_.irradianceSampleDelta, 0.001f, 0.01f, "%.4f")) changed = true;

    if (ImGui::Button("Regenerate IBL"))
    {
      iblSystem_.requestRegeneration(settings_, skybox_);
    }

    // ImGui::End();
  }
} // namespace engine
