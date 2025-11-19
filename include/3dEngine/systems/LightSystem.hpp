#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "../Camera.hpp"
#include "../Device.hpp"
#include "../FrameInfo.hpp"
#include "../GameObject.hpp"
#include "../Model.hpp"
#include "../Pipeline.hpp"
#include "../SwapChain.hpp"

namespace engine {

  class LightSystem
  {
  public:
    LightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~LightSystem();

    // delete copy operations
    LightSystem(const LightSystem&)            = delete;
    LightSystem& operator=(const LightSystem&) = delete;

    void render(FrameInfo& frameInfo);
    void update(FrameInfo& frameInfo, GlobalUbo& ubo) const;

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createDirectionalLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createDirectionalLightPipeline(VkRenderPass renderPass);
    void createSpotLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createSpotLightPipeline(VkRenderPass renderPass);

    Device& device;

    // Point light rendering
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    // Directional light rendering
    std::unique_ptr<Pipeline> directionalPipeline;
    VkPipelineLayout          directionalPipelineLayout;

    // Spot light rendering
    std::unique_ptr<Pipeline> spotPipeline;
    VkPipelineLayout          spotPipelineLayout;
  };
} // namespace engine
