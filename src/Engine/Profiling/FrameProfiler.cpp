#include "Engine/Profiling/FrameProfiler.hpp"

#include <algorithm>
#include <chrono>

namespace engine {

    namespace {
        uint64_t steadyNowNs() {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }
        double nsToMs(uint64_t ns) {
            return static_cast<double>(ns) / 1'000'000.0;
        }
    }  // namespace

    FrameProfiler& FrameProfiler::instance() {
        static FrameProfiler profiler;
        return profiler;
    }

    void FrameProfiler::beginFrame() {
        frameCount_++;
        frameStartNs_ = steadyNowNs();
        currentSections_.clear();
    }

    void FrameProfiler::endFrame() {
        const uint64_t endNs   = steadyNowNs();
        const uint64_t elapsed = (endNs >= frameStartNs_) ? (endNs - frameStartNs_) : 0;
        const float    dtMs    = static_cast<float>(nsToMs(elapsed));

        lastFrameTimeMs_   = dtMs;
        lastFrameSections_ = std::move(currentSections_);
        currentSections_.clear();

        pushHistory(dtMs);
        updateStats();
    }

    void FrameProfiler::beginSection(const char* name) {
        sectionStartNs_ = steadyNowNs();
        // Store placeholder; duration filled on endSection
        (void) name;
    }

    void FrameProfiler::endSection(const char* name) {
        const uint64_t endNs   = steadyNowNs();
        const uint64_t elapsed = (endNs >= sectionStartNs_) ? (endNs - sectionStartNs_) : 0;
        const double   startMs = nsToMs(sectionStartNs_ - frameStartNs_);
        const double   durMs   = nsToMs(elapsed);
        currentSections_.push_back(CpuSection{name, startMs, durMs});
    }

    void FrameProfiler::pushHistory(float dtMs) {
        if (frameTimeHistory_.empty()) {
            frameTimeHistory_.resize(kMaxHistoryFrames, 0.0f);
        }
        frameTimeHistory_[historyWriteIndex_] = dtMs;
        historyWriteIndex_++;
        if (historyWriteIndex_ >= kMaxHistoryFrames) {
            historyWriteIndex_ = 0;
            historyWrapped_    = true;
        }
    }

    void FrameProfiler::updateStats() {
        const size_t count = historyWrapped_ ? kMaxHistoryFrames : historyWriteIndex_;
        if (count == 0)
            return;

        const float dtMs = lastFrameTimeMs_;

        // EMA smoothing
        if (emaFrameTimeMs_ <= 0.0f) {
            emaFrameTimeMs_ = dtMs;
        } else {
            emaFrameTimeMs_ = kEmaAlpha * dtMs + (1.0f - kEmaAlpha) * emaFrameTimeMs_;
        }
        smoothedFrameTimeMs_ = emaFrameTimeMs_;
        smoothedFps_         = (emaFrameTimeMs_ > 0.0f) ? (1000.0f / emaFrameTimeMs_) : 0.0f;

        // Min/max/avg over the history window
        float sum    = 0.0f;
        float curMin = 9999.0f;
        float curMax = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            const float v = frameTimeHistory_[i];
            sum += v;
            if (v < curMin)
                curMin = v;
            if (v > curMax)
                curMax = v;
        }
        avgFrameTimeMs_ = sum / static_cast<float>(count);
        minFrameTimeMs_ = curMin;
        maxFrameTimeMs_ = curMax;
    }

}  // namespace engine