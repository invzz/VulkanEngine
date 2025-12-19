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
  bool                              lastGpuAvailable_ = false;
  std::vector<double>              lastGpuPassMs_;
  std::vector<std::string>         lastGpuPassNames_;

  // Runtime enabled flag
  bool enabled_ = false;

  // GPU query pool and per-frame state
  VkQueryPool                       timestampQueryPool_{VK_NULL_HANDLE};
  uint32_t                          queriesPerFrame_ = 32; // configurable
  uint32_t                          maxFramesInFlight_ = 0;
  VkDevice                          deviceHandle_{VK_NULL_HANDLE};
  double                            timestampPeriodNs_ = 1.0; // ns per tick

  // Per-frame ordered names for boundaries and used count
  std::vector<std::vector<std::string>> perFrameNames_;
  std::vector<uint32_t>                 perFrameUsedCount_; 

public:
  // Profiler initialization for GPU timestamps
  void initGpuQueryPool(VkDevice device, const VkPhysicalDeviceProperties &props, uint32_t maxFramesInFlight, uint32_t queriesPerFrame = 32);

  // Called at the start of a frame recording (resets queries for that frame)
  void beginFrameRecording(VkCommandBuffer cmd, uint32_t frameIndex);

  // Mark a named GPU timestamp boundary in the given command buffer for the given frame
  void markGpuTimestamp(VkCommandBuffer cmd, uint32_t frameIndex, const std::string &name, VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  // Try to collect query results for an older frame (non-blocking). Returns true if results were collected.
  bool tryCollectResultsForFrame(uint32_t frameIndex);

};

} // namespace engine
