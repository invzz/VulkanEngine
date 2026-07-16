#include "Editor/ui/Panels/AtmosphericPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"

namespace engine {

    AtmosphericPanel::AtmosphericPanel(SkyboxSettings& settings)
        : settings_(settings) {}

    void AtmosphericPanel::render(FrameInfo& /*frameInfo*/) {
        if (!ImGui::Begin("Atmospheric Parameters", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::End();
            return;
        }

        ImGui::TextColored(ui::UI::GetAccentColor(), "%s Atmospheric", ICON_FA_CLOUD);
        ImGui::Separator();

        drawSkyOptions();
        ImGui::Separator();
        drawScatteringParams();
        ImGui::Separator();
        drawScaleHeights();
        ImGui::Separator();
        drawSunParams();

        ImGui::End();
    }

    void AtmosphericPanel::drawSkyOptions() {
        ImGui::Text("Sky Options");

        // Procedural Sky toggle
        ImGui::Checkbox("Procedural Sky", &settings_.proceduralSky);
        if (settings_.proceduralSky) {
            ImGui::SameLine();
            ImGui::TextDisabled("(enable to show additional options)");

            // Use Sky LUT
            ImGui::Checkbox("Use Sky LUT", &settings_.useSkyLUT);

            // Time of Day
            ImGui::SliderFloat("Time of Day", &settings_.timeOfDay, 0.0f, 24.0f, "%.1f h");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0 = midnight, 6 = sunrise, 12 = noon, 18 = sunset");

            // Sky Mode combo
            ImGui::Separator();
            ImGui::Text("Sky Mode");
            ImGui::Separator();
            static const char* skyModeLabels[] = {"None", "Procedural", "Cubemap"};
            static int         skyModeCur      = (int) settings_.skyMode;
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##skyMode", &skyModeCur, skyModeLabels, 3)) {
                settings_.skyMode = (SkyMode) skyModeCur;
            }
        } else {
            // Show current sky mode even when procedural is off
            ImGui::Separator();
            ImGui::Text("Sky Mode");
            ImGui::Separator();
            static const char* skyModeLabels[] = {"None", "Procedural", "Cubemap"};
            static int         skyModeCur      = (int) settings_.skyMode;
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##skyModeGlobal", &skyModeCur, skyModeLabels, 3)) {
                settings_.skyMode = (SkyMode) skyModeCur;
            }
        }

        // Debug Cubemap Faces
        ImGui::Separator();
        ImGui::Checkbox("Debug Cubemap Faces", &settings_.debugCubemapFaces);
    }

    void AtmosphericPanel::drawScatteringParams() {
        char buf[64];

        // betaRayleigh R channel
        ImGui::Text("  Beta Rayleigh R (650nm)");
        ImGui::SameLine();
        ImGui::TextDisabled("(5.8e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaRayleigh.x);
        ImGui::InputDouble("##betaRayleighR", &settings_.betaRayleigh.x, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaRayleigh.x = std::max(settings_.betaRayleigh.x, 0.0);
        }

        // betaRayleigh G channel
        ImGui::Text("  Beta Rayleigh G (550nm)");
        ImGui::SameLine();
        ImGui::TextDisabled("(13.0e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaRayleigh.y);
        ImGui::InputDouble("##betaRayleighG", &settings_.betaRayleigh.y, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaRayleigh.y = std::max(settings_.betaRayleigh.y, 0.0);
        }

        // betaRayleigh B channel
        ImGui::Text("  Beta Rayleigh B (440nm)");
        ImGui::SameLine();
        ImGui::TextDisabled("(22.4e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaRayleigh.z);
        ImGui::InputDouble("##betaRayleighB", &settings_.betaRayleigh.z, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaRayleigh.z = std::max(settings_.betaRayleigh.z, 0.0);
        }

        // betaMie R channel
        ImGui::Text("  Beta Mie R");
        ImGui::SameLine();
        ImGui::TextDisabled("(21.0e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaMie.x);
        ImGui::InputDouble("##betaMieR", &settings_.betaMie.x, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaMie.x = std::max(settings_.betaMie.x, 0.0);
        }

        // betaMie G channel
        ImGui::Text("  Beta Mie G");
        ImGui::SameLine();
        ImGui::TextDisabled("(21.0e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaMie.y);
        ImGui::InputDouble("##betaMieG", &settings_.betaMie.y, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaMie.y = std::max(settings_.betaMie.y, 0.0);
        }

        // betaMie B channel
        ImGui::Text("  Beta Mie B");
        ImGui::SameLine();
        ImGui::TextDisabled("(21.0e-6 default)");
        snprintf(buf, sizeof(buf), "%.4e", settings_.betaMie.z);
        ImGui::InputDouble("##betaMieB", &settings_.betaMie.z, 0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.betaMie.z = std::max(settings_.betaMie.z, 0.0);
        }

        // mieG
        ImGui::Text("  Mie g (asymmetry)");
        ImGui::SameLine();
        ImGui::TextDisabled("(0.76 default)");
        ImGui::DragFloat("##mieG", &settings_.mieG, 0.005f, -1.0f, 1.0f, "%.4f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.mieG = std::clamp(settings_.mieG, -1.0f, 1.0f);
        }
    }

    void AtmosphericPanel::drawScaleHeights() {
        char buf[64];

        // atmosphereRadius
        ImGui::Text("  Atmosphere Radius");
        ImGui::SameLine();
        ImGui::TextDisabled("(6460e3 default)");
        snprintf(buf, sizeof(buf), "%.0f", settings_.atmosphereRadius);
        ImGui::InputDouble("##atmosphereRadius", &settings_.atmosphereRadius, 1000.0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.atmosphereRadius = std::max(settings_.atmosphereRadius, 0.0);
        }

        // rayleighScaleHeight
        ImGui::Text("  Rayleigh Scale Height");
        ImGui::SameLine();
        ImGui::TextDisabled("(8000 default)");
        snprintf(buf, sizeof(buf), "%.0f", settings_.rayleighScaleHeight);
        ImGui::InputDouble("##rayleighScaleHeight", &settings_.rayleighScaleHeight, 10.0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.rayleighScaleHeight = std::max(settings_.rayleighScaleHeight, 0.0);
        }

        // mieScaleHeight
        ImGui::Text("  Mie Scale Height");
        ImGui::SameLine();
        ImGui::TextDisabled("(1200 default)");
        snprintf(buf, sizeof(buf), "%.0f", settings_.mieScaleHeight);
        ImGui::InputDouble("##mieScaleHeight", &settings_.mieScaleHeight, 10.0, 0, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.mieScaleHeight = std::max(settings_.mieScaleHeight, 0.0);
        }
    }

    void AtmosphericPanel::drawSunParams() {
        char buf[64];

        // sunIntensity
        ImGui::Text("  Sun Intensity");
        ImGui::SameLine();
        ImGui::TextDisabled("(22.0 default)");
        snprintf(buf, sizeof(buf), "%.1f", settings_.sunIntensity);
        ImGui::DragFloat("##sunIntensity", &settings_.sunIntensity, 0.5f, 0.0f, 100.0f, buf);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.sunIntensity = std::max(settings_.sunIntensity, 0.0f);
        }

        ImGui::Spacing();

        // Latitude
        ImGui::Text("  Latitude");
        ImGui::SameLine();
        ImGui::TextDisabled("(0.0 default, deg)");
        ImGui::SliderFloat("##latitude", &settings_.latitude, -90.0f, 90.0f, "%.1f deg");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Negative = southern hemisphere. Affects sunrise/sunset compass drift.");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.latitude = std::clamp(settings_.latitude, -90.0f, 90.0f);
        }

        // Day of Year
        ImGui::Text("  Day of Year");
        ImGui::SameLine();
        ImGui::TextDisabled("(172 default, ~Jun 21)");
        ImGui::SliderInt("##dayOfYear", &settings_.dayOfYear, 1, 365);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drives seasonal declination (day 172 ~ summer solstice, day 355 ~ winter solstice)");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.dayOfYear = std::clamp(settings_.dayOfYear, 1, 365);
        }
    }

}  // namespace engine
