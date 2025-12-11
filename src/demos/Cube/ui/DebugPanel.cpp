#include "DebugPanel.hpp"

#include <imgui.h>

namespace engine {
  DebugPanel::DebugPanel(int& debugMode) : debugMode_{debugMode} {}

  void DebugPanel::render(FrameInfo& frameInfo)
  {
    // ImGui::Begin("Debug Settings");

    const char* debugItems[] = {"None", "Albedo", "Normal", "Roughness", "Metallic", "Lighting Only", "AO"};
    ImGui::Combo("Debug View", &debugMode_, debugItems, IM_ARRAYSIZE(debugItems));

    // ImGui::End();
  }
} // namespace engine
