#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/GameObject.hpp"

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

    // Update target-locked light rotation (call when light position or target changes)
    static void updateTargetLockedLight(GameObject& obj);

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
