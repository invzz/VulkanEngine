#include "ProfilingPanel.hpp"

#include <imgui.h>
#include <algorithm>

namespace engine {

ProfilingPanel::ProfilingPanel() {}

void ProfilingPanel::render(FrameInfo& /*frameInfo*/)
{
  ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  auto& profiler = Profiler::instance();

  // Controls
  ImGui::Text("Profiling Controls");
  if (!profiler.isProfiling())
  {
    if (ImGui::Button("Start Profiling"))
    {
      profiler.startProfiling();
    }
  }
  else
  {
    if (ImGui::Button("Stop Profiling"))
    {
      profiler.stopProfiling();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Data"))
    {
      profiler.clearRecent();
    }
  }

  ImGui::Separator();

  ImGui::Text("Display");
  ImGui::Checkbox("Show CPU", &showCpu_);
  ImGui::Checkbox("Show GPU", &showGpu_);
  ImGui::Checkbox("Auto Scroll", &autoScroll_);
  ImGui::SliderFloat("CPU spike threshold (ms)", &cpuSpikeThreshold_, 1.0f, 1000.0f, "%.1f ms");
  ImGui::SliderFloat("GPU spike threshold (ms)", &gpuSpikeThreshold_, 1.0f, 1000.0f, "%.1f ms");

  ImGui::Separator();

  // Snapshot recent frames
  std::deque<FrameProfile> frames = profiler.getRecentFrames();
  const int frameCount = static_cast<int>(frames.size());
  if (frameCount == 0)
  {
    ImGui::Text("No profiling data yet.");
    ImGui::End();
    return;
  }

  // Determine GPU pass names (union)
  std::vector<std::string> gpuPassNames;
  for (auto& f : frames)
  {
    for (auto& n : f.gpuPassNames)
    {
      if (std::find(gpuPassNames.begin(), gpuPassNames.end(), n) == gpuPassNames.end())
        gpuPassNames.push_back(n);
    }
  }

  // Build per-pass series (oldest->newest)
  std::vector<std::vector<float>> gpuSeries(gpuPassNames.size(), std::vector<float>(frameCount, 0.0f));
  std::vector<float> cpuSeries(frameCount, 0.0f);
  std::vector<float> gpuTotal(frameCount, 0.0f);

  for (int i = 0; i < frameCount; ++i)
  {
    const auto& f = frames[i];
    cpuSeries[i] = static_cast<float>(f.cpuFrameMs);
    float total = 0.0f;
    for (size_t p = 0; p < gpuPassNames.size(); ++p)
    {
      auto it = std::find(f.gpuPassNames.begin(), f.gpuPassNames.end(), gpuPassNames[p]);
      if (it != f.gpuPassNames.end())
      {
        size_t idx = std::distance(f.gpuPassNames.begin(), it);
        float v = static_cast<float>(f.gpuPassMs[idx]);
        gpuSeries[p][i] = v;
        total += v;
      }
      else
      {
        gpuSeries[p][i] = 0.0f;
      }
    }
    gpuTotal[i] = total;
  }

  // Optionally auto scroll to latest - we use ImGui plot(s) which will show full history but we can show only last N
  const int maxFramesToShow = 256;
  int startIndex = 0;
  if (frameCount > maxFramesToShow)
  {
    if (autoScroll_)
      startIndex = frameCount - maxFramesToShow;
    else
      startIndex = 0;
  }
  int showCount = std::min(frameCount, maxFramesToShow);

  // CPU plot
  if (showCpu_)
  {
    ImGui::Text("CPU frame ms (last %d frames)", showCount);
    ImGui::PlotLines("", cpuSeries.data() + startIndex, showCount, 0, nullptr, 0.0f, *std::max_element(cpuSeries.begin() + startIndex, cpuSeries.end()), ImVec2(600, 80));
  }

  // GPU per pass lines
  if (showGpu_)
  {
    ImGui::Text("GPU per-pass (last %d frames)", showCount);
    for (size_t p = 0; p < gpuPassNames.size(); ++p)
    {
      ImGui::Text("%s", gpuPassNames[p].c_str());
      ImGui::PlotLines((std::string("##gpupass_") + std::to_string(p)).c_str(), gpuSeries[p].data() + startIndex, showCount, 0, nullptr, 0.0f, *std::max_element(gpuSeries[p].begin() + startIndex, gpuSeries[p].end()), ImVec2(600, 60));
    }

    ImGui::Text("GPU total (stacked view as total)");
    ImGui::PlotLines("##gputotal", gpuTotal.data() + startIndex, showCount, 0, nullptr, 0.0f, *std::max_element(gpuTotal.begin() + startIndex, gpuTotal.end()), ImVec2(600, 80));
  }

  // Spike list
  ImGui::Separator();
  ImGui::Text("Spikes (CPU > %.1f ms OR GPU total > %.1f ms)", cpuSpikeThreshold_, gpuSpikeThreshold_);

  // Collect spikes (most recent first)
  struct Spike { int frame; float cpu; float gpu; };
  std::vector<Spike> spikes;
  for (int i = frameCount - 1; i >= 0; --i)
  {
    float cpu = cpuSeries[i];
    float gpu = gpuTotal[i];
    if (cpu > cpuSpikeThreshold_ || gpu > gpuSpikeThreshold_)
    {
      spikes.push_back({frames[i].frameNumber, cpu, gpu});
    }
    if (spikes.size() >= 20) break;
  }

  if (spikes.empty())
  {
    ImGui::Text("No spikes found.");
  }
  else
  {
    ImGui::BeginChild("SpikeList", ImVec2(0, 200), true);
    ImGui::Columns(3);
    ImGui::Text("Frame"); ImGui::NextColumn();
    ImGui::Text("CPU ms"); ImGui::NextColumn();
    ImGui::Text("GPU ms"); ImGui::NextColumn();
    ImGui::Separator();
    for (auto& s : spikes)
    {
      ImGui::Text("%d", s.frame); ImGui::NextColumn();
      ImGui::Text("%.2f", s.cpu); ImGui::NextColumn();
      ImGui::Text("%.2f", s.gpu); ImGui::NextColumn();
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

} // namespace engine