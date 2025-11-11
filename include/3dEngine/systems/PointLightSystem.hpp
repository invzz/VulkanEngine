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

  class PointLightSystem
  {
  public:
    PointLightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~PointLightSystem();

    // delete copy operations
    PointLightSystem(const PointLightSystem&)            = delete;
    PointLightSystem& operator=(const PointLightSystem&) = delete;

    void render(FrameInfo& frameInfo);
    void update(FrameInfo& frameInfo, GlobalUbo& ubo) const;

  private:
    void                      createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void                      createPipeline(VkRenderPass renderPass);
    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;
  };
} // namespace engine
