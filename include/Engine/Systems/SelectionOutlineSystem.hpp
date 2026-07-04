#pragma once
#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Pipeline.hpp"
namespace engine {
    class Scene;
    class FrameInfo;
    class SelectionOutlineSystem {
       public:
        SelectionOutlineSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~SelectionOutlineSystem();
        void render(FrameInfo& frameInfo) const;

       private:
        Device&                   device_;
        std::unique_ptr<Pipeline> pipeline_;
        VkPipelineLayout          pipelineLayout_ = VK_NULL_HANDLE;
    };
}  // namespace engine
