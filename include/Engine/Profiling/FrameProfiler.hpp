#ifndef ENGINE_PROFILING_FRAMEPROFILER_HPP
#define ENGINE_PROFILING_FRAMEPROFILER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

struct CpuSection {
    std::string name;
    double      startMs;  // time since frame start
    double      durationMs;
};

/**
 * CPU-side frame profiler.
 *
 * Provides frame-time history, running statistics, and scoped per-section
 * CPU timing. Complements GpuProfiler (which handles per-render-pass
 * CPU+GPU timestamps via Vulkan queries).
 *
 * Thread-compatible: all calls must come from the main thread.
 */
class FrameProfiler {
   public:
    static constexpr size_t kMaxHistoryFrames = 200;

    static FrameProfiler& instance();

    void beginFrame();
    void endFrame();

    /// Start a named CPU section (manual begin/end, non-nesting).
    void beginSection(const char* name);
    void endSection(const char* name);

    // --- Stats (from the history window) ---

    [[nodiscard]] float smoothedFps() const {
        return smoothedFps_;
    }
    [[nodiscard]] float smoothedFrameTimeMs() const {
        return smoothedFrameTimeMs_;
    }
    [[nodiscard]] float minFrameTimeMs() const {
        return minFrameTimeMs_;
    }
    [[nodiscard]] float maxFrameTimeMs() const {
        return maxFrameTimeMs_;
    }
    [[nodiscard]] float avgFrameTimeMs() const {
        return avgFrameTimeMs_;
    }

    // --- Last-frame data ---

    [[nodiscard]] float lastFrameTimeMs() const {
        return lastFrameTimeMs_;
    }
    [[nodiscard]] const std::vector<CpuSection>& lastFrameSections() const {
        return lastFrameSections_;
    }
    [[nodiscard]] uint64_t frameCount() const {
        return frameCount_;
    }

    // --- History for graphs ---

    [[nodiscard]] const std::vector<float>& frameTimeHistory() const {
        return frameTimeHistory_;
    }
    [[nodiscard]] size_t historySize() const {
        return historyWriteIndex_;
    }

   private:
    FrameProfiler() = default;

    void updateStats();
    void pushHistory(float dtMs);

    // History ring buffer
    std::vector<float> frameTimeHistory_;
    size_t             historyWriteIndex_{0};
    bool               historyWrapped_{false};

    // Running stats (recomputed each frame from history)
    float smoothedFps_{0.0f};
    float smoothedFrameTimeMs_{0.0f};
    float minFrameTimeMs_{9999.0f};
    float maxFrameTimeMs_{0.0f};
    float avgFrameTimeMs_{0.0f};

    // Smoothing (EMA)
    float emaFrameTimeMs_{0.0f};
    static constexpr float kEmaAlpha = 0.1f;

    // Last frame
    float                   lastFrameTimeMs_{0.0f};
    std::vector<CpuSection> lastFrameSections_;

    // Current frame scratch
    double                  frameStartNs_{0};
    double                  sectionStartNs_{0};
    std::vector<CpuSection> currentSections_;

    uint64_t frameCount_{0};
};

/// RAII helper for scoped CPU profiling sections.
class ScopedCpuSection {
   public:
    ScopedCpuSection(const char* name) : name_(name) {
        FrameProfiler::instance().beginSection(name_);
    }
    ~ScopedCpuSection() {
        FrameProfiler::instance().endSection(name_);
    }
    ScopedCpuSection(const ScopedCpuSection&)            = delete;
    ScopedCpuSection& operator=(const ScopedCpuSection&) = delete;

   private:
    const char* name_;
};

}  // namespace engine

#endif