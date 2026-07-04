#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_GPUPROFILER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_GPUPROFILER_HPP
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace engine {
    struct PassTiming {
        std::string passName;
        double      cpuMs{0.0};
        double      gpuMs{-1.0};
    };
    class GpuProfiler {
       public:
        static GpuProfiler&                          instance();
        bool                                         initialize(VkDevice device, float timestampPeriodNs, uint32_t framesInFlight, uint32_t maxPassesPerFrame = 32);
        void                                         shutdown();
        void                                         setEnabled(bool enabled);
        [[nodiscard]] bool                           isEnabled() const;
        void                                         beginFrame(uint64_t frameIndex, VkCommandBuffer commandBuffer);
        void                                         beginPass(std::string_view passName);
        void                                         endPass();
        void                                         endFrame();
        [[nodiscard]] uint64_t                       lastFrameIndex() const;
        [[nodiscard]] double                         lastFrameCpuMs() const;
        [[nodiscard]] double                         lastFrameGpuMs() const;
        [[nodiscard]] const std::vector<PassTiming>& lastFramePassTimings() const;
        [[nodiscard]] bool                           exportLastFrameCsv(const std::string& outputPath, std::string* errorMessage = nullptr) const;
        [[nodiscard]] bool                           exportLastFrameJson(const std::string& outputPath, std::string* errorMessage = nullptr) const;

       private:
        GpuProfiler() = default;
        bool                                  enabled_{false};
        bool                                  initialized_{false};
        VkDevice                              device_{VK_NULL_HANDLE};
        VkQueryPool                           queryPool_{VK_NULL_HANDLE};
        float                                 timestampPeriodNs_{0.0f};
        uint32_t                              framesInFlight_{0};
        uint32_t                              maxPassesPerFrame_{0};
        uint32_t                              queriesPerFrame_{0};
        uint64_t                              currentFrameIndex_{0};
        uint32_t                              currentFrameSlot_{0};
        uint64_t                              lastFrameIndex_{0};
        VkCommandBuffer                       currentCommandBuffer_{VK_NULL_HANDLE};
        bool                                  passActive_{false};
        std::string                           activePassName_;
        uint32_t                              activePassBeginQueryIndex_{0};
        uint32_t                              currentQueryCount_{0};
        std::vector<std::string>              currentPassNames_;
        std::vector<PassTiming>               currentPassTimings_;
        std::vector<PassTiming>               lastPassTimings_;
        std::vector<uint32_t>                 slotQueryCounts_;
        std::vector<uint64_t>                 slotFrameIndices_;
        std::vector<std::vector<std::string>> slotPassNames_;
        std::vector<std::vector<double>>      slotPassCpuMs_;
        std::vector<bool>                     slotHasData_;
        std::vector<double>                   slotCpuFrameMs_;
        double                                currentFrameCpuMs_{0.0};
        double                                lastFrameCpuMs_{0.0};
        double                                lastFrameGpuMs_{-1.0};
        uint64_t                              frameBeginNs_{0};
        uint64_t                              passBeginNs_{0};
        bool                                  resolveSlotGpuTimings(uint32_t slot);
    };
}  // namespace engine
#endif