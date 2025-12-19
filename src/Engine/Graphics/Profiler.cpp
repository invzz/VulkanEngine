#include "Engine/Graphics/Profiler.hpp"
#include "Engine/Graphics/Device.hpp"
#include <iomanip>
#include <filesystem>

namespace engine {

Profiler& Profiler::instance()
{
  static Profiler s;
  return s;
}

Profiler::Profiler()
{
#ifdef PROFILE_OUTPUT_DIR
  std::filesystem::path outDir(PROFILE_OUTPUT_DIR);
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  std::filesystem::path outFile = outDir / "profile.csv";
  csvFile_.open(outFile.string(), std::ios::out | std::ios::trunc);
#else
  csvFile_.open("profile.csv", std::ios::out | std::ios::trunc);
#endif

  enabled_ = false;
}

Profiler::~Profiler()
{
  // Destroy query pool if created
  if (timestampQueryPool_ != VK_NULL_HANDLE && deviceHandle_ != VK_NULL_HANDLE)
  {
    vkDestroyQueryPool(deviceHandle_, timestampQueryPool_, nullptr);
    timestampQueryPool_ = VK_NULL_HANDLE;
  }

  flush();
  if (csvFile_.is_open())
    csvFile_.close();
}

void Profiler::startCpuFrame()
{
  if (!enabled_) return;
  cpuStart_ = std::chrono::high_resolution_clock::now();
  // Clear any per-frame CPU pass data
  std::lock_guard<std::mutex> lg(mutex_);
  currentCpuPasses_.clear();
  activeCpuPasses_.clear();
  // Clear last GPU data as well (it will be set by collect)
  lastGpuAvailable_ = false;
  lastGpuPassMs_.clear();
  lastGpuPassNames_.clear();
}

void Profiler::initGpuQueryPool(VkDevice device, const VkPhysicalDeviceProperties &props, uint32_t maxFramesInFlight, uint32_t queriesPerFrame)
{
  std::lock_guard<std::mutex> lg(mutex_);
  if (timestampQueryPool_ != VK_NULL_HANDLE) return; // already inited

  deviceHandle_ = device;
  maxFramesInFlight_ = maxFramesInFlight;
  queriesPerFrame_ = queriesPerFrame;
  timestampPeriodNs_ = static_cast<double>(props.limits.timestampPeriod);

  VkQueryPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  poolInfo.queryCount = maxFramesInFlight_ * queriesPerFrame_;

  if (vkCreateQueryPool(deviceHandle_, &poolInfo, nullptr, &timestampQueryPool_) != VK_SUCCESS)
  {
    timestampQueryPool_ = VK_NULL_HANDLE;
    std::cerr << "Failed to create timestamp query pool" << std::endl;
    return;
  }

  perFrameNames_.assign(maxFramesInFlight_, {});
  perFrameUsedCount_.assign(maxFramesInFlight_, 0);
}

void Profiler::beginFrameRecording(VkCommandBuffer cmd, uint32_t frameIndex)
{
  if (!enabled_ || timestampQueryPool_ == VK_NULL_HANDLE) return;
  std::lock_guard<std::mutex> lg(mutex_);
  if (frameIndex >= maxFramesInFlight_) return;

  // Reset per-frame state
  perFrameNames_[frameIndex].clear();
  perFrameUsedCount_[frameIndex] = 0;

  // Reset the query pool range for this frame in the GPU command buffer
  const uint32_t baseQuery = frameIndex * queriesPerFrame_;
  vkCmdResetQueryPool(cmd, timestampQueryPool_, baseQuery, queriesPerFrame_);
}

void Profiler::markGpuTimestamp(VkCommandBuffer cmd, uint32_t frameIndex, const std::string &name, VkPipelineStageFlagBits stage)
{
  if (!enabled_ || timestampQueryPool_ == VK_NULL_HANDLE) return;
  std::lock_guard<std::mutex> lg(mutex_);
  if (frameIndex >= maxFramesInFlight_) return;

  uint32_t localIdx = perFrameUsedCount_[frameIndex];
  if (localIdx >= queriesPerFrame_)
  {
    // no more queries available for this frame
    return;
  }

  perFrameNames_[frameIndex].push_back(name);
  perFrameUsedCount_[frameIndex] = localIdx + 1;

  const uint32_t globalIdx = frameIndex * queriesPerFrame_ + localIdx;
  vkCmdWriteTimestamp(cmd, stage, timestampQueryPool_, globalIdx);
}

bool Profiler::tryCollectResultsForFrame(uint32_t frameIndex)
{
  if (!enabled_ || timestampQueryPool_ == VK_NULL_HANDLE) return false;
  std::lock_guard<std::mutex> lg(mutex_);
  if (frameIndex >= maxFramesInFlight_) return false;
  uint32_t used = perFrameUsedCount_[frameIndex];
  if (used <= 1) return false; // need at least two points to measure durations

  std::vector<uint64_t> results(used);
  const uint32_t baseQuery = frameIndex * queriesPerFrame_;

  VkResult r = vkGetQueryPoolResults(
      deviceHandle_,
      timestampQueryPool_,
      baseQuery,
      used,
      sizeof(uint64_t) * used,
      results.data(),
      sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT);

  if (r == VK_NOT_READY)
    return false; // not yet available
  if (r != VK_SUCCESS)
  {
    // unexpected error
    return false;
  }

  // Convert to ms deltas and build labels
  std::vector<double> gpuMs;
  std::vector<std::string> gpuNames;
  for (uint32_t i = 1; i < used; ++i)
  {
    uint64_t delta = results[i] - results[i - 1];
    double ms = static_cast<double>(delta) * timestampPeriodNs_ / 1e6;
    gpuMs.push_back(ms);
    // label is the name of the later boundary (perFrameNames_[frameIndex][i])
    gpuNames.push_back(perFrameNames_[frameIndex][i]);
  }

  // Write to CSV and make available for CPU frame attach
  logGpuFrame(frameIndex, gpuMs, gpuNames);
  return true;
}

void Profiler::endCpuFrame()
{
  if (!enabled_) return;

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> diff = end - cpuStart_;
  std::lock_guard<std::mutex> lg(mutex_);
  currentFrame_++;

  // Write CSV line (frame and cpu)
  if (csvFile_.is_open())
  {
    csvFile_ << currentFrame_ << "," << std::fixed << std::setprecision(3) << diff.count();
    // gpu data will be appended by logGpuFrame() which would have written its values earlier in the same line
    // If no GPU values were written for this frame, ensure csv still has consistent columns
    if (headerWritten_)
    {
      // pad GPU columns if none were present
      for (std::size_t i = 0; i < headerGpuCount_; ++i)
      {
        csvFile_ << ",";
      }
      csvFile_ << std::endl;
    }
  }

  // Build FrameProfile and push to ring buffer
  FrameProfile fp;
  fp.frameNumber = currentFrame_;
  fp.cpuFrameMs  = diff.count();

  for (auto &p : currentCpuPasses_)
  {
    fp.cpuPassNames.push_back(p.first);
    fp.cpuPassMs.push_back(p.second);
  }

  // Attach GPU data if it was just recorded
  if (lastGpuAvailable_)
  {
    fp.gpuPassMs = lastGpuPassMs_;
    fp.gpuPassNames = lastGpuPassNames_;
    lastGpuAvailable_ = false; // consume
  }

  recentFrames_.push_back(std::move(fp));
  if (recentFrames_.size() > kRecentFrames)
    recentFrames_.pop_front();
}

void Profiler::startCpuPass(const std::string& name)
{
  if (!enabled_) return;
  std::lock_guard<std::mutex> lg(mutex_);
  activeCpuPasses_[name] = std::chrono::high_resolution_clock::now();
}

void Profiler::endCpuPass(const std::string& name)
{
  if (!enabled_) return;
  auto end = std::chrono::high_resolution_clock::now();
  std::lock_guard<std::mutex> lg(mutex_);
  auto it = activeCpuPasses_.find(name);
  if (it == activeCpuPasses_.end())
  {
    // unmatched end, ignore
    return;
  }
  std::chrono::duration<double, std::milli> diff = end - it->second;
  currentCpuPasses_[name] = diff.count();
  activeCpuPasses_.erase(it);
}

void Profiler::logGpuFrame(int frameNumber, const std::vector<double>& gpuPassMs, const std::vector<std::string>& gpuPassNames)
{
  if (!enabled_) return;

  std::lock_guard<std::mutex> lg(mutex_);
  if (!csvFile_.is_open())
    return;

  // If header not written yet, write header that matches the number of GPU pass columns
  if (!headerWritten_)
  {
    csvFile_ << "frame,cpu_ms";
    headerGpuCount_ = gpuPassMs.size();
    for (std::size_t i = 0; i < headerGpuCount_; ++i)
    {
      // Prefer human readable names if provided
      if (!gpuPassNames.empty() && i < gpuPassNames.size())
      {
        csvFile_ << "," << gpuPassNames[i];
      }
      else
      {
        csvFile_ << ",gpu_pass_" << i;
      }
    }
    csvFile_ << std::endl;
    headerWritten_ = true;
  }

  // Write GPU pass values for this frame, padding missing values to keep column counts consistent
  for (std::size_t i = 0; i < gpuPassMs.size(); ++i)
  {
    csvFile_ << "," << std::fixed << std::setprecision(3) << gpuPassMs[i];
  }
  for (std::size_t i = gpuPassMs.size(); i < headerGpuCount_; ++i)
  {
    csvFile_ << ","; // pad empty value
  }

  csvFile_ << std::endl;

  // Store last GPU info to be attached at endCpuFrame
  lastGpuAvailable_ = true;
  lastGpuPassMs_ = gpuPassMs;
  lastGpuPassNames_.clear();
  if (!gpuPassNames.empty())
    lastGpuPassNames_ = gpuPassNames;
}

std::deque<FrameProfile> Profiler::getRecentFrames()
{
  std::lock_guard<std::mutex> lg(mutex_);
  return recentFrames_; // copy under lock
}

void Profiler::startProfiling()
{
  std::lock_guard<std::mutex> lg(mutex_);
  if (enabled_) return;
  enabled_ = true;
  // reset CSV
  if (csvFile_.is_open())
  {
    csvFile_.close();
  }
#ifdef PROFILE_OUTPUT_DIR
  std::filesystem::path outDir(PROFILE_OUTPUT_DIR);
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  std::filesystem::path outFile = outDir / "profile.csv";
  csvFile_.open(outFile.string(), std::ios::out | std::ios::trunc);
#else
  csvFile_.open("profile.csv", std::ios::out | std::ios::trunc);
#endif
  headerWritten_ = false;
  headerGpuCount_ = 0;
  currentFrame_ = 0;
  recentFrames_.clear();
}

void Profiler::stopProfiling()
{
  std::lock_guard<std::mutex> lg(mutex_);
  if (!enabled_) return;
  enabled_ = false;
  if (csvFile_.is_open())
    csvFile_.flush();
}

bool Profiler::isProfiling() const
{
  return enabled_;
}

void Profiler::clearRecent()
{
  std::lock_guard<std::mutex> lg(mutex_);
  recentFrames_.clear();
}

void Profiler::flush()
{
  std::lock_guard<std::mutex> lg(mutex_);
  if (csvFile_.is_open())
    csvFile_.flush();
}

} // namespace engine
