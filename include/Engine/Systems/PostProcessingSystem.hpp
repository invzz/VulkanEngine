#pragma once

#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

  struct PostProcessPushConstants
  {
    float     exposure{1.0f};
    float     contrast{1.0f};
    float     saturation{1.0f};
    float     vignette{0.4f};
    float     bloomIntensity{0.04f};
    float     bloomThreshold{1.0f};
    int       enableFXAA{1};
    int       enableBloom{1};
    float     fxaaSpanMax{8.0f};
    float     fxaaReduceMul{0.125f};
    float     fxaaReduceMin{0.0078125f};
    int       enableSSAO{1};
    float     ssaoRadius{0.5f};
    float     ssaoBias{0.025f};
    int       toneMappingMode{1}; // 0: None, 1: ACES
    float     _padding{0.0f};
    glm::mat4 inverseProjection{1.0f};
    glm::mat4 projection{1.0f};
  };

  class PostProcessingSystem
  {
  public:
    PostProcessingSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts);
    ~PostProcessingSystem();

    PostProcessingSystem(const PostProcessingSystem&)            = delete;
    PostProcessingSystem& operator=(const PostProcessingSystem&) = delete;

    void render(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, const PostProcessPushConstants& push);

  private:
    void createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts);
    void createPipeline(VkRenderPass renderPass);

    Device& device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;
  };
} // namespace engine
