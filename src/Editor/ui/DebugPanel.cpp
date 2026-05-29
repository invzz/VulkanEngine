#include "Editor/ui/DebugPanel.hpp"

#include <imgui.h>

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

  if (ImGui::BeginCombo("Debug View", debugOptions[selectedIndex].label)) {
    for (int i = 0; i < IM_ARRAYSIZE(debugOptions); ++i) {
      bool const isSelected = (debugOptions[i].value == debugMode_);
      if (ImGui::Selectable(debugOptions[i].label, isSelected)) {
        debugMode_ = debugOptions[i].value;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (debugMode_ == 7 || debugMode_ == 8) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "How to verify HZB culling:");
    ImGui::BulletText("Culled meshlets are NOT rendered at all");
    ImGui::BulletText("You cannot 'see' them because they don't exist in the frame");
    ImGui::Spacing();
    ImGui::Text("Verification methods:");
    ImGui::BulletText("Compare FPS with HZB on/off (indoor scenes)");
    ImGui::BulletText("Use RenderDoc to count mesh shader dispatches");
    ImGui::BulletText("Watch for 'popping' at screen edges if HZB too aggressive");
  }
  if (debugMode_ == 9) {
    ImGui::TextWrapped("Shows linearized depth. Objects closer to camera are darker. This is what HZB uses for occlusion decisions.");
  }

  if (debugMode_ == 10) {
    ImGui::TextWrapped("Shows per-fragment ambient occlusion (AO). Darker = more occluded.");
  }

  // Debug mode 6: Emissive only
  if (debugMode_ == 6) {
    ImGui::TextWrapped("Shows emissive contribution only (material emissive textures/colors). If the scene has no emissive materials this view will be dark.");
  }

  // Debug mode 12: IBL irradiance visualization
  if (debugMode_ == 12) {
    ImGui::TextWrapped("Shows IBL diffuse irradiance (sampled from irradiance cube). Use this to verify diffuse IBL content and orientation.");
  }

  // Debug mode 13: IBL specular prefilter visualization
  if (debugMode_ == 13) {
    ImGui::TextWrapped("Shows IBL specular prefiltered map sample (roughness dependent). Brightness/blur should vary with roughness.");
  }

  // Debug mode 14: BRDF LUT visualization
  if (debugMode_ == 14) {
    ImGui::TextWrapped("Shows BRDF LUT sample (used to compute specular energy). Expect a smooth 1D-like gradient across NdotV/roughness.");
  }
}
}  // namespace engine
