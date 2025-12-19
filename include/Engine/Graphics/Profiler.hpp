#pragma once

#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

struct FrameProfile
{
  int frameNumber;
  double cpuFrameMs;
  // CPU named passes
  std::vector<std::string> cpuPassNames;
  std::vector<double>      cpuPassMs;
  // GPU pass timings (order corresponds to passNames)
  std::vector<std::string> gpuPassNames;
  std::vector<double>      gpuPassMs;
};

class Profiler
{
public:
  static Profiler& instance();

  void startCpuFrame();
  void endCpuFrame();

  // Named CPU pass helpers
  void startCpuPass(const std::string& name);
  void endCpuPass(const std::string& name);

  // Log GPU frame timings (called by Renderer)
  void logGpuFrame(int frameNumber, const std::vector<double>& gpuPassMs, const std::vector<std::string>& gpuPassNames = {});

  // Runtime control
  void startProfiling();
  void stopProfiling();
  bool isProfiling() const;
  void clearRecent();

  // Access recent frames snapshot for UI (returns a copy to avoid races)
  std::deque<FrameProfile> getRecentFrames();

  void flush();

private:
  Profiler();
  ~Profiler();

  std::mutex mutex_;
  std::ofstream csvFile_;

  // temporary holders for current frame CPU time
  std::chrono::high_resolution_clock::time_point cpuStart_;
  int currentFrame_ = 0;

  bool headerWritten_ = false;
  std::size_t headerGpuCount_ = 0;

  // Named CPU passes for current frame
  std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> activeCpuPasses_;
  std::unordered_map<std::string, double> currentCpuPasses_;

  // Recent frames ring buffer
  std::deque<FrameProfile> recentFrames_;
  static constexpr size_t kRecentFrames = 1024;

  // Last GPU data waiting to be consumed by endCpuFrame
  int                              lastGpuFrameNumber_ = -1;
  std::vector<double>              lastGpuPassMs_;
  std::vector<std::string>         lastGpuPassNames_;
};

} // namespace engine
