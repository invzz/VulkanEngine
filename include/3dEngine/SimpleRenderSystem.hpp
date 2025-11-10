#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "Camera.hpp"
#include "Device.hpp"
#include "FrameInfo.hpp"
#include "GameObject.hpp"
#include "Model.hpp"
#include "Pipeline.hpp"
#include "SwapChain.hpp"

namespace engine {

  class SimpleRenderSystem
  {
  public:
    SimpleRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    // delete copy operations
    SimpleRenderSystem(const SimpleRenderSystem&)            = delete;
    SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

    void renderGameObjects(FrameInfo& frameInfo, const std::vector<GameObject>& gameObjects);

  private:
    void                      createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void                      createPipeline(VkRenderPass renderPass);
    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;
  };
} // namespace engine
