#pragma once
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../Camera.hpp"
#include "../Descriptors.hpp"
#include "../Device.hpp"
#include "../FrameInfo.hpp"
#include "../GameObject.hpp"
#include "../Model.hpp"
#include "../Pipeline.hpp"
#include "../SwapChain.hpp"

namespace engine {

  // Forward declarations
  class MaterialSystem;

  class PBRRenderSystem
  {
  public:
    PBRRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~PBRRenderSystem();

    PBRRenderSystem(const PBRRenderSystem&)            = delete;
    PBRRenderSystem& operator=(const PBRRenderSystem&) = delete;

    void render(FrameInfo& frameInfo);

    // Clear the material descriptor cache (call when materials are modified)
    void clearDescriptorCache();

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    // Material management subsystem
    std::unique_ptr<MaterialSystem> materialSystem;
  };
} // namespace engine
