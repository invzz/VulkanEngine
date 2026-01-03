#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace engine {

  class Device;
  class FrameBuffer;

  class HZBGenerator
  {
  public:
    explicit HZBGenerator(Device& device);
    ~HZBGenerator();

    HZBGenerator(const HZBGenerator&)            = delete;
    HZBGenerator& operator=(const HZBGenerator&) = delete;

    void recreateDescriptors(FrameBuffer& frameBuffer, VkExtent2D extent, uint32_t framesInFlight);

    void generate(VkCommandBuffer commandBuffer, FrameBuffer& frameBuffer, VkExtent2D extent, int frameIndex);

  private:
    void createPipelineIfNeeded();
    void destroyDescriptorPool();

    static uint32_t calculateMipLevels(VkExtent2D extent);

    Device& device_;

    VkPipelineLayout      hzbPipelineLayout_{VK_NULL_HANDLE};
    VkPipeline            hzbPipeline_{VK_NULL_HANDLE};
    VkDescriptorSetLayout hzbSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool      hzbDescriptorPool_{VK_NULL_HANDLE};

    uint32_t mipLevels_{0};
    uint32_t framesInFlight_{0};

    // Outer: Frame index, Inner: Mip index
    std::vector<std::vector<VkDescriptorSet>> hzbDescriptorSets_;
  };

} // namespace engine
