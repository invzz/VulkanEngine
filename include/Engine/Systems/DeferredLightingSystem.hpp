#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_DEFERREDLIGHTINGSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_DEFERREDLIGHTINGSYSTEM_HPP

#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

    class DeferredLightingSystem {
       public:
        DeferredLightingSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts);
        ~DeferredLightingSystem();

        DeferredLightingSystem(const DeferredLightingSystem&)            = delete;
        DeferredLightingSystem& operator=(const DeferredLightingSystem&) = delete;

        void render(FrameInfo& frameInfo, VkDescriptorSet globalSet, VkDescriptorSet gbufferSet, VkDescriptorSet shadowSet, VkDescriptorSet iblSet);

       private:
        void createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts);
        void createPipeline(VkRenderPass renderPass);

        Device&                   device;
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout          pipelineLayout{VK_NULL_HANDLE};
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_DEFERREDLIGHTINGSYSTEM_HPP
