#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_POSTPROCESSINGSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_POSTPROCESSINGSYSTEM_HPP

#include <cstddef>
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
    int       debugMode{0};
    float     ssaoRadius{0.5f};
    float     ssaoBias{0.025f};
    int       toneMappingMode{1}; // 0: None, 1: ACES
    glm::vec4 sunScreenPos;       // xy = screen pos [0,1], z = isVisible (1.0/0.0), w = padding
    int       bakedRaw{0};        // 0 = display mode, 1 = raw linear display for baked debug
    float     godRayDensity{1.0f};
    float     godRayWeight{0.01f};
    float     godRayDecay{1.0f};
    float     godRayExposure{1.0f};
    alignas(16) glm::mat4 inverseProjection{1.0f};
    alignas(16) glm::mat4 projection{1.0f};
  };

  // Compile-time checks to ensure C++ push-constant layout matches GLSL expectations
  static_assert(offsetof(PostProcessPushConstants, sunScreenPos) % 16 == 0, "PostProcessPushConstants::sunScreenPos must be 16-byte aligned for GLSL vec4 alignment");
  static_assert(offsetof(PostProcessPushConstants, sunScreenPos) == 64, "Unexpected offset for sunScreenPos; does GLSL push layout match C++?");
  static_assert(offsetof(PostProcessPushConstants, godRayExposure) == 96, "Unexpected offset for godRayExposure; does GLSL push layout match C++?");
  static_assert(offsetof(PostProcessPushConstants, inverseProjection) == 112, "Unexpected offset for inverseProjection; does GLSL push layout match C++?");
  static_assert(sizeof(PostProcessPushConstants) == 240, "Unexpected PostProcessPushConstants size; expected 240 bytes per std140-like packing");
  static_assert(sizeof(PostProcessPushConstants) <= 256, "PostProcessPushConstants size exceeds 256 bytes and may exceed typical GPU push constant limits");

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

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_POSTPROCESSINGSYSTEM_HPP
