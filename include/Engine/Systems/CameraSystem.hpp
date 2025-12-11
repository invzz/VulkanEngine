#pragma once

#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  class CameraSystem
  {
  public:
    CameraSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~CameraSystem();

    void update(FrameInfo& frameInfo, float aspectRatio) const;
    void render(FrameInfo& frameInfo) const;

  private:
    void updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio) const;
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    Device&                   device;
    VkPipelineLayout          pipelineLayout;
    std::unique_ptr<Pipeline> pipeline;
  };

} // namespace engine
