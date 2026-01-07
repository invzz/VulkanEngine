#include "CubeUI/ui/DebugPanel.hpp"

#include <imgui.h>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {
  DebugPanel::DebugPanel(int& debugMode) : debugMode_{debugMode} {}

  void DebugPanel::render(FrameInfo& /*frameInfo*/)
  {
    const char* debugItems[] = {"None", "Albedo", "Normal", "Roughness", "Metallic", "Lighting Only", "AO", "Meshlets", "Meshlet Cones", "Depth"};
    ImGui::Combo("Debug View", &debugMode_, debugItems, IM_ARRAYSIZE(debugItems));

    if (debugMode_ == 7 || debugMode_ == 8)
    {
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
    if (debugMode_ == 9)
    {
      ImGui::TextWrapped("Shows linearized depth. Objects closer to camera are darker. This is what HZB uses for occlusion decisions.");
    }
  }
} // namespace engine
