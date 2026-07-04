#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_CAMERASYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_CAMERASYSTEM_HPP
#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
namespace engine {
    class CameraSystem {
       public:
        CameraSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~CameraSystem();
        static void update(FrameInfo& frameInfo, float aspectRatio);
        void        render(FrameInfo& frameInfo) const;

       private:
        static void               updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio);
        void                      createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void                      createPipeline(VkRenderPass renderPass);
        Device&                   device;
        VkPipelineLayout          pipelineLayout;
        std::unique_ptr<Pipeline> pipeline;
    };
}  // namespace engine
#endif
