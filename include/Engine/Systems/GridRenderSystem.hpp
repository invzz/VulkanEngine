#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_GRIDRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_GRIDRENDERSYSTEM_HPP
#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
namespace engine {
    class GridRenderSystem {
       public:
        GridRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~GridRenderSystem();
        GridRenderSystem(const GridRenderSystem&)            = delete;
        GridRenderSystem& operator=(const GridRenderSystem&) = delete;
        void              render(FrameInfo& frameInfo) const;

       private:
        void                      createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void                      createPipeline(VkRenderPass renderPass);
        Device&                   device_;
        std::unique_ptr<Pipeline> pipeline_;
        VkPipelineLayout          pipelineLayout_ = VK_NULL_HANDLE;
    };
}  // namespace engine
#endif
