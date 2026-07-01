#include "Editor/ui/PostProcessPanel.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

#include "Editor/ui/UI.hpp"

namespace engine {
    PostProcessPanel::PostProcessPanel(PostProcessPushConstants& pushConstants) : pushConstants{pushConstants} {}

    void PostProcessPanel::render(FrameInfo& /*frameInfo*/) {
        ui::UI::BeginSurface("pp_tone", "Color Grading", "Global tone and transfer controls");
        ui::UI::DragFloat("Exposure##pp_exposure", &pushConstants.exposure, 0.01f, 0.1f, 10.0f);
        ui::UI::DragFloat("Contrast##pp_contrast", &pushConstants.contrast, 0.01f, 0.1f, 2.0f);
        ui::UI::DragFloat("Saturation##pp_saturation", &pushConstants.saturation, 0.01f, 0.0f, 2.0f);
        ui::UI::DragFloat("Vignette##pp_vignette", &pushConstants.vignette, 0.01f, 0.0f, 5.0f);
        const char* toneMappingItems[] = {"None", "ACES Filmic"};
        ui::UI::Combo("Tone Mapping##pp_tone_mapping", &pushConstants.toneMappingMode, toneMappingItems, IM_ARRAYSIZE(toneMappingItems));
        ui::UI::EndSurface();

        ui::UI::BeginSurface("pp_bloom", "Bloom", "Soft highlights and glow response");
        bool bloom = pushConstants.enableBloom == 1;
        if (ui::UI::CheckboxRow("Enable Bloom", "Adds glow around bright pixels", &bloom)) {
            pushConstants.enableBloom = bloom ? 1 : 0;
        }
        if (bloom) {
            ui::UI::DragFloat("Bloom Intensity##pp_bloom_intensity", &pushConstants.bloomIntensity, 0.001f, 0.0f, 1.0f);
            ui::UI::DragFloat("Bloom Threshold##pp_bloom_threshold", &pushConstants.bloomThreshold, 0.01f, 0.0f, 5.0f);
        }
        ui::UI::EndSurface();

        ui::UI::BeginSurface("pp_fxaa", "Anti-Aliasing", "Edge cleanup for post-processing");
        bool fxaa = pushConstants.enableFXAA == 1;
        if (ui::UI::CheckboxRow("Enable FXAA", "Fast approximate anti-aliasing", &fxaa)) {
            pushConstants.enableFXAA = fxaa ? 1 : 0;
        }
        if (fxaa) {
            ui::UI::DragFloat("FXAA Span Max##pp_fxaa_span", &pushConstants.fxaaSpanMax, 0.1f, 1.0f, 16.0f);
            ui::UI::DragFloat("FXAA Reduce Mul##pp_fxaa_reduce_mul", &pushConstants.fxaaReduceMul, 0.001f, 0.0f, 1.0f);
            ui::UI::DragFloat("FXAA Reduce Min##pp_fxaa_reduce_min", &pushConstants.fxaaReduceMin, 0.0001f, 0.0f, 0.1f);
        }
        ui::UI::EndSurface();

        ui::UI::BeginSurface("pp_ssao", "SSAO", "Screen-space ambient occlusion");
        bool ssao = pushConstants.enableSSAO == 1;
        if (ui::UI::CheckboxRow("Enable SSAO", "Adds small-scale contact shadows", &ssao)) {
            pushConstants.enableSSAO = ssao ? 1 : 0;
        }
        if (ssao) {
            ui::UI::DragFloat("SSAO Radius##pp_ssao_radius", &pushConstants.ssaoRadius, 0.01f, 0.0f, 2.0f);
            ui::UI::DragFloat("SSAO Bias##pp_ssao_bias", &pushConstants.ssaoBias, 0.001f, 0.0f, 0.5f);
        }
        ui::UI::EndSurface();

        if (ui::UI::TonalButton("Reset Post FX##pp_reset")) {
            pushConstants.exposure        = 1.0f;
            pushConstants.contrast        = 1.0f;
            pushConstants.saturation      = 1.0f;
            pushConstants.vignette        = 0.4f;
            pushConstants.enableBloom     = 1;
            pushConstants.bloomIntensity  = 0.04f;
            pushConstants.bloomThreshold  = 1.0f;
            pushConstants.enableFXAA      = 1;
            pushConstants.fxaaSpanMax     = 8.0f;
            pushConstants.fxaaReduceMul   = 0.125f;
            pushConstants.fxaaReduceMin   = 0.0078125f;
            pushConstants.enableSSAO      = 1;
            pushConstants.ssaoRadius      = 0.5f;
            pushConstants.ssaoBias        = 0.025f;
            pushConstants.toneMappingMode = 1;
        }
    }
}  // namespace engine
