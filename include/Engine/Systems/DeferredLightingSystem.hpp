#pragma once

#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

  class DeferredLightingSystem
  {
  public:
    DeferredLightingSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts);
    ~DeferredLightingSystem();

    DeferredLightingSystem(const DeferredLightingSystem&)            = delete;
    DeferredLightingSystem& operator=(const DeferredLightingSystem&) = delete;

    void render(FrameInfo& frameInfo, VkDescriptorSet globalSet, VkDescriptorSet gbufferSet, VkDescriptorSet iblSet, VkDescriptorSet shadowSet);

  private:
    void createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts);
    void createPipeline(VkRenderPass renderPass);

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout{VK_NULL_HANDLE};
  };

} // namespace engine
