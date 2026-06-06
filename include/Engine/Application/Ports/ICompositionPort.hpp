#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace engine {

    struct FrameInfo;
    class PostProcessingSystem;
    class PostProcessPushConstants;

    // Port for composition pass without knowing EngineState or Editor internals.
    class ICompositionPort {
       public:
        virtual ~ICompositionPort() = default;

        [[nodiscard]] virtual PostProcessingSystem*     getPostProcessingSystem()                                                                                 = 0;
        [[nodiscard]] virtual PostProcessPushConstants& getPostProcessPush()                                                                                      = 0;
        [[nodiscard]] virtual VkDescriptorSet           getPostProcessDescriptorSet(uint32_t frameIndex)                                                          = 0;
        virtual void                                    renderPostProcessing(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, PostProcessPushConstants& push) = 0;
        virtual void                                    renderUI(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool enabled)                               = 0;
    };

}  // namespace engine
