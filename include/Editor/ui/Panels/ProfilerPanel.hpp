#ifndef EDITOR_PROFILERPANEL_HPP
#define EDITOR_PROFILERPANEL_HPP

#include "Editor/ui/UIPanel.hpp"

namespace engine {

class FrameProfiler;
class GpuProfiler;

/**
 * Real-time profiling panel.
 *
 * Displays FPS/frame-time graph, per-pass CPU+GPU breakdown, and
 * running statistics. Reads from FrameProfiler (history) and
 * GpuProfiler (per-pass timings).
 */
class ProfilerPanel : public UIPanel {
   public:
    ProfilerPanel();
    void render(FrameInfo& frameInfo) override;

   private:
    void drawFpsGraph();
    void drawStatsTable();
    void drawPassBreakdown();
    void drawCpuSections();

    // Graph state
    float graphMaxMs_{16.67f};  // auto-scales
    bool  graphAutoScale_{true};
    bool  graphPaused_{false};
};

}  // namespace engine

#endif