#include "Editor/ui/DebugPanel.hpp"

#include <imgui.h>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {
DebugPanel::DebugPanel(int& debugMode) : debugMode_{debugMode} {}

void DebugPanel::render(FrameInfo& /*frameInfo*/) {
  // Debug mode indices: 0=None, 1=Albedo, 2=Normal, 3=Roughness, 4=Metallic,
  // 5=Lighting, 6=Emissive, 7=Meshlets, 8=Meshlet Cones, 9=Depth, 10=AO,
  // 11=reserved, 12=IBL Irradiance, 13=IBL Prefilter, 14=IBL BRDF LUT,
  // 15=CSM Cascades, 16=View Depth, 17=CSM Split Compare, 18=Raw Depth Hue
  const char* debugItems[] = {"None",
      "Albedo",
      "Normal",
      "Roughness",
      "Metallic",
      "Lighting Only",
      "Emissive Only",
      "Meshlets",
      "Meshlet Cones",
      "Depth",
      "AO",
      "(Reserved)",
      "IBL: Irradiance",
      "IBL: Prefilter",
      "IBL: BRDF LUT",
      "CSM: Cascades",
      "CSM: View Depth",
      "CSM: Split Compare",
      "CSM: Raw Depth"};
  ImGui::Combo("Debug View", &debugMode_, debugItems, IM_ARRAYSIZE(debugItems));

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

  // Debug mode 6: Emissive only (not baked lightmaps)
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

  // Debug mode 15: CSM cascade visualization
  if (debugMode_ == 15) {
    ImGui::TextWrapped("Shows which shadow cascade covers each pixel: Red=0 (near), Green=1, Blue=2, Yellow=3 (far). Cascades should transition based on distance from camera.");
  }

  // Debug mode 16: View-space depth visualization
  if (debugMode_ == 16) {
    ImGui::TextWrapped("Shows view-space depth used for CSM cascade selection. Darker=near, Lighter=far. Normalized by the last cascade split distance.");
  }

  // Debug mode 17: CSM split comparison
  if (debugMode_ == 17) {
    ImGui::TextWrapped("Shows cascade split comparisons: R=depth>split0, G=depth>split1, B=depth>split2. Black=cascade0, Red=cascade1, Yellow=cascade2, White=cascade3.");
  }

  // Debug mode 18: Raw depth hue
  if (debugMode_ == 18) {
    ImGui::TextWrapped("Shows raw view depth as hue (Blue=near, Green=mid, Red=far). Max range 200 units. Use to verify depth scale.");
  }

  // Debug mode 19: Per-cascade shadow samples
  if (debugMode_ == 19) {
    ImGui::TextWrapped("Shows sampled shadow values per cascade (R= cascade0, G=cascade1, B=cascade2). Useful to detect empty/over-biased cascades.");
  }
}
}  // namespace engine
