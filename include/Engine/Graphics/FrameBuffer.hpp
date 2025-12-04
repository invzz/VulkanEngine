#pragma once

#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  class FrameBuffer
  {
  public:
    struct Attachment
    {
      VkFormat          format;
      VkImageUsageFlags usage;
      VkImageLayout     finalLayout;
    };

    FrameBuffer(Device& device, VkExtent2D extent, uint32_t frameCount, bool useMipmaps = false);
    ~FrameBuffer();

    FrameBuffer(const FrameBuffer&)            = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    void resize(VkExtent2D newExtent);

    VkRenderPass          getRenderPass() const { return renderPass; }
    VkDescriptorImageInfo getDescriptorImageInfo(int index) const;

    void beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
    void endRenderPass(VkCommandBuffer commandBuffer) const;
    void generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex);

    float getAspectRatio() const { return static_cast<float>(extent.width) / static_cast<float>(extent.height); }

  private:
    void createRenderPass();
    void createImages();
    void createFramebuffers();
    void cleanup();

    Device&    device;
    VkExtent2D extent;
    uint32_t   frameCount;
    bool       useMipmaps;
    uint32_t   mipLevels{1};

    VkRenderPass renderPass{VK_NULL_HANDLE};

    // Color attachment
    std::vector<VkImage>        colorImages;
    std::vector<VkDeviceMemory> colorImageMemorys;
    std::vector<VkImageView>    colorImageViews;
    std::vector<VkImageView>    colorAttachmentImageViews;

    // Depth attachment
    std::vector<VkImage>        depthImages;
    std::vector<VkDeviceMemory> depthImageMemorys;
    std::vector<VkImageView>    depthImageViews;

    std::vector<VkFramebuffer> framebuffers;
    VkSampler                  sampler{VK_NULL_HANDLE};
  };

} // namespace engine
