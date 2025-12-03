#include "PostProcessPanel.hpp"

#include <imgui.h>

namespace engine {
  PostProcessPanel::PostProcessPanel(PostProcessPushConstants& pushConstants) : pushConstants{pushConstants} {}

  void PostProcessPanel::render(FrameInfo& frameInfo)
  {
    ImGui::Begin("Post Processing");

    ImGui::DragFloat("Exposure", &pushConstants.exposure, 0.01f, 0.1f, 10.0f);
    ImGui::DragFloat("Contrast", &pushConstants.contrast, 0.01f, 0.1f, 2.0f);
    ImGui::DragFloat("Saturation", &pushConstants.saturation, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Vignette", &pushConstants.vignette, 0.01f, 0.0f, 5.0f);

    ImGui::Separator();
    ImGui::Text("Bloom");
    bool bloom = pushConstants.enableBloom == 1;
    if (ImGui::Checkbox("Enable Bloom", &bloom))
    {
      pushConstants.enableBloom = bloom ? 1 : 0;
    }
    if (bloom)
    {
      ImGui::DragFloat("Bloom Intensity", &pushConstants.bloomIntensity, 0.001f, 0.0f, 1.0f);
      ImGui::DragFloat("Bloom Threshold", &pushConstants.bloomThreshold, 0.01f, 0.0f, 5.0f);
    }

    ImGui::Separator();
    ImGui::Text("Anti-Aliasing");
    bool fxaa = pushConstants.enableFXAA == 1;
    if (ImGui::Checkbox("Enable FXAA", &fxaa))
    {
      pushConstants.enableFXAA = fxaa ? 1 : 0;
    }
    if (fxaa)
    {
      ImGui::DragFloat("FXAA Span Max", &pushConstants.fxaaSpanMax, 0.1f, 1.0f, 16.0f);
      ImGui::DragFloat("FXAA Reduce Mul", &pushConstants.fxaaReduceMul, 0.001f, 0.0f, 1.0f);
      ImGui::DragFloat("FXAA Reduce Min", &pushConstants.fxaaReduceMin, 0.0001f, 0.0f, 0.1f);
    }

    if (ImGui::Button("Reset"))
    {
      pushConstants.exposure       = 1.0f;
      pushConstants.contrast       = 1.0f;
      pushConstants.saturation     = 1.0f;
      pushConstants.vignette       = 0.4f;
      pushConstants.enableBloom    = 1;
      pushConstants.bloomIntensity = 0.04f;
      pushConstants.bloomThreshold = 1.0f;
      pushConstants.enableFXAA     = 1;
      pushConstants.fxaaSpanMax    = 8.0f;
      pushConstants.fxaaReduceMul  = 0.125f;
      pushConstants.fxaaReduceMin  = 0.0078125f;
    }

    ImGui::End();
  }
} // namespace engine
