#include "Engine/Graphics/GpuProfiler.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <vector>

namespace engine {

    namespace {

        uint64_t steadyNowNs() {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        double nsToMs(uint64_t ns) {
            return static_cast<double>(ns) / 1000000.0;
        }

    }  // namespace

    GpuProfiler& GpuProfiler::instance() {
        static GpuProfiler profiler;
        return profiler;
    }

    bool GpuProfiler::initialize(VkDevice device, float timestampPeriodNs, uint32_t framesInFlight, uint32_t maxPassesPerFrame) {
        shutdown();

        if (device == VK_NULL_HANDLE || framesInFlight == 0 || maxPassesPerFrame == 0) {
            return false;
        }

        device_            = device;
        timestampPeriodNs_ = timestampPeriodNs;
        framesInFlight_    = framesInFlight;
        maxPassesPerFrame_ = maxPassesPerFrame;
        queriesPerFrame_   = maxPassesPerFrame_ * 2;

        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = framesInFlight_ * queriesPerFrame_;

        if (vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool_) != VK_SUCCESS) {
            shutdown();
            return false;
        }

        slotQueryCounts_.assign(framesInFlight_, 0);
        slotFrameIndices_.assign(framesInFlight_, 0);
        slotPassNames_.assign(framesInFlight_, {});
        slotPassCpuMs_.assign(framesInFlight_, {});
        slotHasData_.assign(framesInFlight_, false);
        slotCpuFrameMs_.assign(framesInFlight_, 0.0);

        initialized_ = true;
        return true;
    }

    void GpuProfiler::shutdown() {
        if (queryPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_, queryPool_, nullptr);
        }

        queryPool_   = VK_NULL_HANDLE;
        device_      = VK_NULL_HANDLE;
        initialized_ = false;

        slotQueryCounts_.clear();
        slotFrameIndices_.clear();
        slotPassNames_.clear();
        slotPassCpuMs_.clear();
        slotHasData_.clear();
        slotCpuFrameMs_.clear();

        passActive_           = false;
        currentCommandBuffer_ = VK_NULL_HANDLE;
        currentQueryCount_    = 0;
    }

    void GpuProfiler::setEnabled(bool enabled) {
        enabled_ = enabled;
    }

    bool GpuProfiler::isEnabled() const {
        return enabled_;
    }

    void GpuProfiler::beginFrame(uint64_t frameIndex, VkCommandBuffer commandBuffer) {
        if (!enabled_ || !initialized_) {
            return;
        }

        currentFrameIndex_    = frameIndex;
        currentFrameSlot_     = static_cast<uint32_t>(frameIndex % framesInFlight_);
        currentCommandBuffer_ = commandBuffer;

        if (slotHasData_[currentFrameSlot_]) {
            (void) resolveSlotGpuTimings(currentFrameSlot_);
        }

        currentFrameCpuMs_ = 0.0;
        currentPassTimings_.clear();
        currentPassNames_.clear();
        passActive_ = false;
        activePassName_.clear();
        currentQueryCount_ = 0;

        const uint32_t base = currentFrameSlot_ * queriesPerFrame_;
        vkCmdResetQueryPool(currentCommandBuffer_, queryPool_, base, queriesPerFrame_);

        frameBeginNs_ = steadyNowNs();
    }

    void GpuProfiler::beginPass(std::string_view passName) {
        if (!enabled_ || !initialized_) {
            return;
        }

        if (passActive_) {
            endPass();
        }

        activePassName_ = std::string(passName);
        passBeginNs_    = steadyNowNs();
        passActive_     = true;

        currentPassNames_.push_back(activePassName_);

        if (currentQueryCount_ + 2 <= queriesPerFrame_) {
            activePassBeginQueryIndex_ = (currentFrameSlot_ * queriesPerFrame_) + currentQueryCount_;
            vkCmdWriteTimestamp(currentCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, activePassBeginQueryIndex_);
            currentQueryCount_ += 2;
        } else {
            activePassBeginQueryIndex_ = UINT32_MAX;
        }
    }

    void GpuProfiler::endPass() {
        if (!enabled_ || !initialized_ || !passActive_) {
            return;
        }

        const uint64_t endNs     = steadyNowNs();
        const uint64_t elapsedNs = (endNs >= passBeginNs_) ? (endNs - passBeginNs_) : 0;
        if (activePassBeginQueryIndex_ != UINT32_MAX) {
            vkCmdWriteTimestamp(currentCommandBuffer_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_, activePassBeginQueryIndex_ + 1);
        }

        currentPassTimings_.push_back(PassTiming{.passName = activePassName_, .cpuMs = nsToMs(elapsedNs), .gpuMs = -1.0});

        passActive_ = false;
        activePassName_.clear();
    }

    void GpuProfiler::endFrame() {
        if (!enabled_ || !initialized_) {
            return;
        }

        if (passActive_) {
            endPass();
        }

        const uint64_t endNs     = steadyNowNs();
        const uint64_t elapsedNs = (endNs >= frameBeginNs_) ? (endNs - frameBeginNs_) : 0;
        currentFrameCpuMs_       = nsToMs(elapsedNs);

        slotFrameIndices_[currentFrameSlot_] = currentFrameIndex_;
        slotCpuFrameMs_[currentFrameSlot_]   = currentFrameCpuMs_;
        slotPassNames_[currentFrameSlot_]    = currentPassNames_;
        slotPassCpuMs_[currentFrameSlot_].clear();
        slotPassCpuMs_[currentFrameSlot_].reserve(currentPassTimings_.size());
        for (const auto& timing : currentPassTimings_) {
            slotPassCpuMs_[currentFrameSlot_].push_back(timing.cpuMs);
        }
        slotQueryCounts_[currentFrameSlot_] = currentQueryCount_;
        slotHasData_[currentFrameSlot_]     = true;
    }

    uint64_t GpuProfiler::lastFrameIndex() const {
        return lastFrameIndex_;
    }

    double GpuProfiler::lastFrameCpuMs() const {
        return lastFrameCpuMs_;
    }

    double GpuProfiler::lastFrameGpuMs() const {
        return lastFrameGpuMs_;
    }

    const std::vector<PassTiming>& GpuProfiler::lastFramePassTimings() const {
        return lastPassTimings_;
    }

    bool GpuProfiler::exportLastFrameCsv(const std::string& outputPath, std::string* errorMessage) const {
        std::ofstream out(outputPath, std::ios::trunc);
        if (!out.is_open()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to open output file";
            }
            return false;
        }

        out << "frame_index,total_cpu_ms,total_gpu_ms,pass_name,pass_cpu_ms,pass_gpu_ms\n";
        if (lastPassTimings_.empty()) {
            out << lastFrameIndex_ << "," << lastFrameCpuMs_ << "," << lastFrameGpuMs_ << ",, ,\n";
        } else {
            for (const auto& timing : lastPassTimings_) {
                out << lastFrameIndex_ << "," << lastFrameCpuMs_ << "," << lastFrameGpuMs_ << "," << timing.passName << "," << timing.cpuMs << "," << timing.gpuMs << "\n";
            }
        }

        if (!out.good()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to write CSV contents";
            }
            return false;
        }

        return true;
    }

    bool GpuProfiler::exportLastFrameJson(const std::string& outputPath, std::string* errorMessage) const {
        std::ofstream out(outputPath, std::ios::trunc);
        if (!out.is_open()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to open output file";
            }
            return false;
        }

        out << "{\n";
        out << "  \"frameIndex\": " << lastFrameIndex_ << ",\n";
        out << "  \"totalCpuMs\": " << std::fixed << std::setprecision(6) << lastFrameCpuMs_ << ",\n";
        out << "  \"totalGpuMs\": " << std::fixed << std::setprecision(6) << lastFrameGpuMs_ << ",\n";
        out << "  \"passes\": [\n";

        for (size_t i = 0; i < lastPassTimings_.size(); ++i) {
            const auto& timing = lastPassTimings_[i];
            out << "    {\"name\": \"" << timing.passName << "\", \"cpuMs\": " << std::fixed << std::setprecision(6) << timing.cpuMs << ", \"gpuMs\": " << std::fixed << std::setprecision(6)
                << timing.gpuMs << "}";
            if (i + 1 < lastPassTimings_.size()) {
                out << ",";
            }
            out << "\n";
        }

        out << "  ]\n";
        out << "}\n";

        if (!out.good()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to write JSON contents";
            }
            return false;
        }

        return true;
    }

    bool GpuProfiler::resolveSlotGpuTimings(uint32_t slot) {
        if (!initialized_ || !slotHasData_[slot]) {
            return false;
        }

        const uint32_t queryCount = slotQueryCounts_[slot];
        const uint32_t base       = slot * queriesPerFrame_;

        lastFrameIndex_ = slotFrameIndices_[slot];
        lastFrameCpuMs_ = slotCpuFrameMs_[slot];
        lastFrameGpuMs_ = -1.0;

        lastPassTimings_.clear();
        const auto& passNames = slotPassNames_[slot];
        const auto& passCpuMs = slotPassCpuMs_[slot];
        for (const auto& passName : passNames) {
            lastPassTimings_.push_back(PassTiming{.passName = passName, .cpuMs = 0.0, .gpuMs = -1.0});
        }

        if (queryCount == 0) {
            slotHasData_[slot] = false;
            return true;
        }

        std::vector<uint64_t> timestamps(queryCount, 0);
        const VkResult        result = vkGetQueryPoolResults(
            device_,
            queryPool_,
            base,
            queryCount,
            sizeof(uint64_t) * timestamps.size(),
            timestamps.data(),
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);

        if (result != VK_SUCCESS) {
            for (size_t i = 0; i < lastPassTimings_.size(); ++i) {
                if (i < passCpuMs.size()) {
                    lastPassTimings_[i].cpuMs = passCpuMs[i];
                }
            }
            slotHasData_[slot] = false;
            return false;
        }

        const uint32_t passCountWithGpu = queryCount / 2;
        double         totalGpuMs       = 0.0;
        bool           hasAnyGpuTiming  = false;
        for (size_t i = 0; i < lastPassTimings_.size(); ++i) {
            if (i < passCpuMs.size()) {
                lastPassTimings_[i].cpuMs = passCpuMs[i];
            }
            if (i < passCountWithGpu) {
                const uint64_t beginTs    = timestamps[i * 2];
                const uint64_t endTs      = timestamps[(i * 2) + 1];
                const uint64_t delta      = (endTs >= beginTs) ? (endTs - beginTs) : 0;
                const double   gpuNs      = static_cast<double>(delta) * static_cast<double>(timestampPeriodNs_);
                lastPassTimings_[i].gpuMs = gpuNs / 1000000.0;
                totalGpuMs += lastPassTimings_[i].gpuMs;
                hasAnyGpuTiming = true;
            }
        }

        lastFrameGpuMs_ = hasAnyGpuTiming ? totalGpuMs : -1.0;

        slotHasData_[slot] = false;
        return true;
    }

}  // namespace engine