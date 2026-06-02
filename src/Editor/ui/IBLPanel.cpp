#include "Editor/ui/IBLPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Systems/IBLSystem.hpp"

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
  // ImGui::Begin("IBL Settings");

  bool changed = false;

  // Irradiance Size
  int currentIrradianceSizeIdx = 1;  // Default 64
  if (settings_.irradianceSize == 32) currentIrradianceSizeIdx = 0;
  if (settings_.irradianceSize == 64) currentIrradianceSizeIdx = 1;
  if (settings_.irradianceSize == 128) currentIrradianceSizeIdx = 2;
  if (settings_.irradianceSize == 256) currentIrradianceSizeIdx = 3;

  const char* sizeItems[] = {"32", "64", "128", "256"};
  if (ImGui::Combo("Irradiance Size", &currentIrradianceSizeIdx, sizeItems, IM_ARRAYSIZE(sizeItems))) {
    settings_.irradianceSize = std::stoi(sizeItems[currentIrradianceSizeIdx]);
    changed = true;
  }

  // Prefilter Size
  int currentPrefilterSizeIdx = 2;  // Default 512
  if (settings_.prefilterSize == 128) currentPrefilterSizeIdx = 0;
  if (settings_.prefilterSize == 256) currentPrefilterSizeIdx = 1;
  if (settings_.prefilterSize == 512) currentPrefilterSizeIdx = 2;
  if (settings_.prefilterSize == 1024) currentPrefilterSizeIdx = 3;

  const char* prefilterSizeItems[] = {"128", "256", "512", "1024"};
  if (ImGui::Combo("Prefilter Size", &currentPrefilterSizeIdx, prefilterSizeItems, IM_ARRAYSIZE(prefilterSizeItems))) {
    settings_.prefilterSize = std::stoi(prefilterSizeItems[currentPrefilterSizeIdx]);
    changed = true;
  }

  // Prefilter Mip Levels
  if (ImGui::SliderInt("Prefilter Mip Levels", &settings_.prefilterMipLevels, 1, 10)) changed = true;

  // Sample Count
  int sampleCount = static_cast<int>(
      std::min<uint32_t>(settings_.prefilterSampleCount, static_cast<uint32_t>(std::numeric_limits<int>::max())));
  if (ImGui::InputInt("Prefilter Samples", &sampleCount)) {
    sampleCount = std::clamp(sampleCount, 1, std::numeric_limits<int>::max());
    settings_.prefilterSampleCount = sampleCount;
    changed = true;
  }

  // Sample Delta
  if (ImGui::InputFloat("Irradiance Delta", &settings_.irradianceSampleDelta, 0.001f, 0.01f, "%.4f")) changed = true;

  if (ImGui::Button("Regenerate IBL")) {
    if (engineState_ != nullptr) {
      auto rendering = engineState_->renderingService().view();
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
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load/enable a skybox to generate IBL");
    ImGui::TextDisabled("No skybox loaded; IBL regeneration disabled");
  }

  // ImGui::End();
}
}  // namespace engine
