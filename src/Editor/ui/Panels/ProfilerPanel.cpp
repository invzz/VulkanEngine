#include "Editor/ui/Panels/ProfilerPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Profiling/FrameProfiler.hpp"

namespace engine {

    ProfilerPanel::ProfilerPanel() = default;

    void ProfilerPanel::render(FrameInfo& /*frameInfo*/) {
        if (!ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::End();
            return;
        }

        drawFpsGraph();
        ImGui::Separator();
        drawStatsTable();
        ImGui::Separator();
        drawPassBreakdown();
        ImGui::Separator();
        drawCpuSections();

        ImGui::End();
    }

    void ProfilerPanel::drawFpsGraph() {
        auto& profiler = FrameProfiler::instance();

        // Controls row
        float fps = profiler.smoothedFps();
        float dt  = profiler.smoothedFrameTimeMs();
        ImGui::Text("FPS: %.1f  |  Frame: %.2f ms", fps, dt);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scale", &graphAutoScale_);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            graphMaxMs_ = 16.67f;
        }
        if (!graphAutoScale_) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("Max ms", &graphMaxMs_, 0.5f, 1.0f, 200.0f, "%.1f");
        }

        // Graph
        const auto&  history = profiler.frameTimeHistory();
        const size_t count   = profiler.historySize();
        if (count > 0) {
            // Auto-scale: find max in visible range, clamp to reasonable bounds
            float plotMax = graphMaxMs_;
            if (graphAutoScale_) {
                float histMax = 0.0f;
                for (size_t i = 0; i < count; ++i) {
                    if (history[i] > histMax)
                        histMax = history[i];
                }
                plotMax = std::max(8.33f, histMax * 1.15f);  // at least 120 FPS
                plotMax = std::min(plotMax, 200.0f);         // cap
            }

            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%.1f ms", dt);

            // We need a non-const float array for the offset trick (ring buffer)
            // ImGui PlotLines takes const float*, so we just plot what we have.
            ImGui::PlotLines("##frameTime",
                history.data(),
                static_cast<int>(count),
                0,
                overlay,
                0.0f,
                plotMax,
                ImVec2(-1, 120));
        }

        // Legend
        ImGui::TextDisabled("Green line = 60 FPS (16.67 ms)  |  Red line = 30 FPS (33.33 ms)");
    }

    void ProfilerPanel::drawStatsTable() {
        auto& profiler = FrameProfiler::instance();

        ImGui::Text("Frame Stats (last %zu frames)", profiler.historySize());
        if (ImGui::BeginTable("##stats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Metric");
            ImGui::TableSetupColumn("Min");
            ImGui::TableSetupColumn("Max");
            ImGui::TableSetupColumn("Avg");
            ImGui::TableHeadersRow();

            const float minMs = profiler.minFrameTimeMs();
            const float maxMs = profiler.maxFrameTimeMs();
            const float avgMs = profiler.avgFrameTimeMs();
            const float fps   = profiler.smoothedFps();

            auto row = [](const char* label, const char* fmt, float vMin, float vMax, float vAvg) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(fmt, vMin);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text(fmt, vMax);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text(fmt, vAvg);
            };

            row("Frame (ms)", "%.2f", minMs, maxMs, avgMs);
            row("FPS", "%.0f",
                maxMs > 0 ? 1000.0f / maxMs : 0,
                minMs > 0 ? 1000.0f / minMs : 0,
                fps);

            ImGui::EndTable();
        }
    }

    void ProfilerPanel::drawPassBreakdown() {
        auto& gpuProfiler = GpuProfiler::instance();
        if (!gpuProfiler.isEnabled()) {
            ImGui::TextDisabled("GPU Profiler disabled. Enable in Settings.");
            return;
        }

        const auto& passTimings = gpuProfiler.lastFramePassTimings();
        if (passTimings.empty()) {
            ImGui::TextDisabled("No pass timing data yet.");
            return;
        }

        ImGui::Text("Render Passes (Frame #%llu)", static_cast<unsigned long long>(gpuProfiler.lastFrameIndex()));
        ImGui::Text("  Total CPU: %.3f ms  |  Total GPU: %.3f ms",
            gpuProfiler.lastFrameCpuMs(),
            gpuProfiler.lastFrameGpuMs());

        if (!ImGui::BeginTable("##passes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            return;
        }
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("CPU (ms)");
        ImGui::TableSetupColumn("GPU (ms)");
        ImGui::TableSetupColumn("% Frame");
        ImGui::TableHeadersRow();

        const double totalGpu = gpuProfiler.lastFrameGpuMs();
        for (const auto& timing : passTimings) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(timing.passName.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", timing.cpuMs);

            ImGui::TableSetColumnIndex(2);
            if (timing.gpuMs >= 0.0) {
                ImGui::Text("%.3f", timing.gpuMs);
            } else {
                ImGui::TextDisabled("--");
            }

            ImGui::TableSetColumnIndex(3);
            if (timing.gpuMs >= 0.0 && totalGpu > 0.0) {
                const float pct = static_cast<float>(timing.gpuMs / totalGpu * 100.0);
                ImGui::Text("%.1f%%", pct);

                // Mini bar in the same cell
                ImGui::SameLine();
                ImGui::ProgressBar(pct / 100.0f, ImVec2(60.0f, 0.0f), "");
            } else {
                ImGui::TextDisabled("--");
            }
        }

        ImGui::EndTable();
    }

    void ProfilerPanel::drawCpuSections() {
        auto&       profiler = FrameProfiler::instance();
        const auto& sections = profiler.lastFrameSections();

        if (sections.empty()) {
            return;
        }

        ImGui::Text("CPU Sections");
        if (!ImGui::BeginTable("##cpusections", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            return;
        }
        ImGui::TableSetupColumn("Section");
        ImGui::TableSetupColumn("Duration (ms)");
        ImGui::TableSetupColumn("% Frame");
        ImGui::TableHeadersRow();

        const float frameMs = profiler.lastFrameTimeMs();
        for (const auto& sec : sections) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sec.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", sec.durationMs);

            ImGui::TableSetColumnIndex(2);
            if (frameMs > 0.0f) {
                const float pct = static_cast<float>(sec.durationMs / frameMs * 100.0);
                ImGui::Text("%.1f%%", pct);
                ImGui::SameLine();
                ImGui::ProgressBar(pct / 100.0f, ImVec2(60.0f, 0.0f), "");
            }
        }

        ImGui::EndTable();
    }

}  // namespace engine