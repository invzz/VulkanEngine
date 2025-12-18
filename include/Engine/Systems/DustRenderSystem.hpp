#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

  struct DustSettings
  {
    bool  enabled{true};
    float boxSize{20.0f};
    float particleSize{5.0f};
    float alpha{0.5f};
    float heightFalloff{0.1f};
    int   particleCount{500};
  };

  class DustRenderSystem
  {
  public:
    DustRenderSystem(Device& device, VkRenderPass renderPass);
    ~DustRenderSystem();

    DustRenderSystem(const DustRenderSystem&)            = delete;
    DustRenderSystem& operator=(const DustRenderSystem&) = delete;

    void render(FrameInfo& frameInfo, const DustSettings& settings, const glm::vec4& sunDir, const glm::vec3& sunColor, const glm::vec3& ambientColor);

  private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    void createDustBuffer();

    Device& device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t                vertexCount;
  };

} // namespace engine
