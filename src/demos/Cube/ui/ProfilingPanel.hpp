#pragma once

#include "ui/UIPanel.hpp"
#include "Engine/Graphics/Profiler.hpp"

namespace engine {

class ProfilingPanel : public UIPanel
{
public:
  ProfilingPanel();
  void render(FrameInfo& frameInfo) override;
  bool isSeparateWindow() const override { return true; }

private:
  float cpuSpikeThreshold_ = 50.0f; // ms
  float gpuSpikeThreshold_ = 50.0f; // ms total
  bool showCpu_ = true;
  bool showGpu_ = true;
  bool autoScroll_ = true;
};

} // namespace engine