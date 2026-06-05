#include "Editor/ui/DebugPanel.hpp"

#include <imgui.h>

#include "Editor/UI/UI.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {
DebugPanel::DebugPanel(int& debugMode) : debugMode_{debugMode} {}

void DebugPanel::render(FrameInfo& /*frameInfo*/) {
  struct DebugOption {
    const char* label;
    int value;
  };

  static constexpr DebugOption debugOptions[] = {
      {"None", 0},
      {"Albedo", 1},
      {"Normal", 2},
      {"Roughness", 3},
      {"Metallic", 4},
      {"Lighting Only", 5},
      {"Emissive Only", 6},
      {"Meshlets", 7},
      {"Meshlet Cones", 8},
      {"Depth", 9},
      {"AO", 10},
      {"IBL: Irradiance", 12},
      {"IBL: Prefilter", 13},
      {"IBL: BRDF LUT", 14},
  };

  int selectedIndex = 0;
  for (int i = 0; i < IM_ARRAYSIZE(debugOptions); ++i) {
    if (debugOptions[i].value == debugMode_) {
      selectedIndex = i;
      break;
    }
  }

  // Push theme style
  ui::UI::PushThemeStyle();

  // Combo widget
  std::string comboLabel = "Debug View##debug_combo";
  // Extract labels for combo
  const char* comboLabels[14];
  for (int i = 0; i < 14; ++i) {
    comboLabels[i] = debugOptions[i].label;
  }
  int tmpIdx = selectedIndex;
  if (ui::UI::Combo(comboLabel.c_str(), &tmpIdx, comboLabels, 14)) {
    debugMode_ = debugOptions[tmpIdx].value;
    selectedIndex = tmpIdx;
  }

  // Debug text for specific modes
  if (debugMode_ == 9) {
    ui::UI::TextDisabled("Shows linearized depth. Objects closer to camera are darker.");
  }

  if (debugMode_ == 10) {
    ui::UI::TextDisabled("Shows per-fragment ambient occlusion (AO). Darker = more occluded.");
  }

  if (debugMode_ == 6) {
    ui::UI::TextDisabled("Shows emissive contribution only (material emissive textures/colors). If the scene has no emissive materials this view will be dark.");
  }

  if (debugMode_ == 12) {
    ui::UI::TextDisabled("Shows IBL diffuse irradiance (sampled from irradiance cube). Use this to verify diffuse IBL content and orientation.");
  }

  if (debugMode_ == 13) {
    ui::UI::TextDisabled("Shows IBL specular prefiltered map sample (roughness dependent). Brightness/blur should vary with roughness.");
  }

  if (debugMode_ == 14) {
    ui::UI::TextDisabled("Shows BRDF LUT sample (used to compute specular energy). Expect a smooth 1D-like gradient across NdotV/roughness.");
  }

  ui::UI::PopThemeStyle();
}
}  // namespace engine
