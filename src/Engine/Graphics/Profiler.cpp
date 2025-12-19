#include "Engine/Graphics/Profiler.hpp"
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
  // Clear last GPU data as well (it will be set by logGpuFrame)
  lastGpuFrameNumber_ = -1;
  lastGpuPassMs_.clear();
  lastGpuPassNames_.clear();
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

  // Attach GPU data if present
  if (lastGpuFrameNumber_ == currentFrame_)
  {
    fp.gpuPassMs = lastGpuPassMs_;
    fp.gpuPassNames = lastGpuPassNames_;
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
  lastGpuFrameNumber_ = frameNumber + 1; // endCpuFrame increments currentFrame_ before storing
  lastGpuPassMs_ = gpuPassMs;
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
