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

    // Accessors for HZB
    VkImageView getDepthMipImageView(int frameIndex, int mipLevel) const { return depthMipImageViews[frameIndex][mipLevel]; }
    VkImageView getDepthImageView(int frameIndex) const { return depthImageViews[frameIndex]; }
    VkSampler   getDepthSampler() const { return depthSampler; }
    VkImage     getDepthImage(int frameIndex) const { return depthImages[frameIndex]; }

    // Accessors for HZB Texture (R32_SFLOAT)
    VkImageView getHzbMipImageView(int frameIndex, int mipLevel) const { return hzbMipImageViews[frameIndex][mipLevel]; }
    VkImageView getHzbImageView(int frameIndex) const { return hzbImageViews[frameIndex]; }
    VkImage     getHzbImage(int frameIndex) const { return hzbImages[frameIndex]; }
    VkSampler   getHzbSampler() const { return hzbSampler; }

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
    // Per-mip views for depth (for HZB generation)
    // Outer vector: frame index, Inner vector: mip level
    std::vector<std::vector<VkImageView>> depthMipImageViews;

    // HZB attachment (R32_SFLOAT)
    std::vector<VkImage>                  hzbImages;
    std::vector<VkDeviceMemory>           hzbImageMemorys;
    std::vector<VkImageView>              hzbImageViews;
    std::vector<std::vector<VkImageView>> hzbMipImageViews;

    std::vector<VkFramebuffer> framebuffers;
    VkSampler                  sampler{VK_NULL_HANDLE};
    VkSampler                  depthSampler{VK_NULL_HANDLE};
    VkSampler                  hzbSampler{VK_NULL_HANDLE};
  };

} // namespace engine
