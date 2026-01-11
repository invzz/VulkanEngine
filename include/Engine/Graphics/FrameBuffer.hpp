#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP

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

    [[nodiscard]] VkRenderPass          getRenderPass() const { return renderPass; }
    [[nodiscard]] VkRenderPass          getDepthPrepassRenderPass() const { return depthPrepassRenderPass; }
    [[nodiscard]] VkRenderPass          getRenderPassLoadDepth() const { return renderPassLoadDepth; }
    [[nodiscard]] VkRenderPass          getRenderPassLoadColorDepth() const { return renderPassLoadColorDepth; }
    [[nodiscard]] VkRenderPass          getGbufferRenderPass() const { return gbufferRenderPass; }
    [[nodiscard]] VkRenderPass          getDeferredLightingRenderPass() const { return deferredLightingRenderPass; }
    [[nodiscard]] uint32_t              getMipLevels() const { return mipLevels; }
    [[nodiscard]] VkDescriptorImageInfo getDescriptorImageInfo(int index) const;
    [[nodiscard]] VkDescriptorImageInfo getSceneColorDescriptorImageInfo(int index) const;
    [[nodiscard]] VkDescriptorImageInfo getGbufferNormalImageInfo(int index) const;
    [[nodiscard]] VkDescriptorImageInfo getGbufferAlbedoImageInfo(int index) const;
    [[nodiscard]] VkDescriptorImageInfo getGbufferMaterialImageInfo(int index) const;
    [[nodiscard]] VkDescriptorImageInfo getGbufferBakedImageInfo(int index) const;

    void        beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
    void        beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
    void        beginRenderPassLoadDepth(VkCommandBuffer commandBuffer, int frameIndex);
    void        beginRenderPassLoadColorDepth(VkCommandBuffer commandBuffer, int frameIndex);
    void        beginGbufferRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
    void        beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
    static void endRenderPass(VkCommandBuffer commandBuffer);
    void        generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex);
    void        generateSceneColorMipmaps(VkCommandBuffer commandBuffer, int frameIndex);

    [[nodiscard]] float getAspectRatio() const { return static_cast<float>(extent.width) / static_cast<float>(extent.height); }

    // Accessors for HZB
    [[nodiscard]] VkImageView getDepthMipImageView(int frameIndex, int mipLevel) const { return depthMipImageViews[frameIndex][mipLevel]; }
    [[nodiscard]] VkImageView getDepthImageView(int frameIndex) const { return depthImageViews[frameIndex]; }
    [[nodiscard]] VkSampler   getDepthSampler() const { return depthSampler; }
    [[nodiscard]] VkImage     getDepthImage(int frameIndex) const { return depthImages[frameIndex]; }

    // Offscreen color image (render target)
    [[nodiscard]] VkImage getColorImage(int frameIndex) const { return colorImages[frameIndex]; }

    // G-buffer attachments
    [[nodiscard]] VkImageView getGbufferNormalImageView(int frameIndex) const { return gbufferNormalImageViews[frameIndex]; }
    [[nodiscard]] VkImageView getGbufferAlbedoImageView(int frameIndex) const { return gbufferAlbedoImageViews[frameIndex]; }
    [[nodiscard]] VkImageView getGbufferMaterialImageView(int frameIndex) const { return gbufferMaterialImageViews[frameIndex]; }

    // Scene color copy (for transmission refraction sampling)
    [[nodiscard]] VkImageView getSceneColorImageView(int frameIndex) const { return sceneColorImageViews[frameIndex]; }
    [[nodiscard]] VkImage     getSceneColorImage(int frameIndex) const { return sceneColorImages[frameIndex]; }

    // Accessors for HZB Texture (R32_SFLOAT)
    [[nodiscard]] VkImageView getHzbMipImageView(int frameIndex, int mipLevel) const { return hzbMipImageViews[frameIndex][mipLevel]; }
    [[nodiscard]] VkImageView getHzbImageView(int frameIndex) const { return hzbImageViews[frameIndex]; }
    [[nodiscard]] VkImage     getHzbImage(int frameIndex) const { return hzbImages[frameIndex]; }
    [[nodiscard]] VkSampler   getHzbSampler() const { return hzbSampler; }

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
    VkRenderPass depthPrepassRenderPass{VK_NULL_HANDLE};
    VkRenderPass renderPassLoadDepth{VK_NULL_HANDLE};
    VkRenderPass renderPassLoadColorDepth{VK_NULL_HANDLE};
    VkRenderPass gbufferRenderPass{VK_NULL_HANDLE};
    VkRenderPass deferredLightingRenderPass{VK_NULL_HANDLE};

    // Color attachment
    std::vector<VkImage>        colorImages;
    std::vector<VkDeviceMemory> colorImageMemorys;
    std::vector<VkImageView>    colorImageViews;
    std::vector<VkImageView>    colorAttachmentImageViews;

    // Scene color copy (sampled in transmission pass)
    std::vector<VkImage>        sceneColorImages;
    std::vector<VkDeviceMemory> sceneColorImageMemorys;
    std::vector<VkImageView>    sceneColorImageViews;

    // G-buffer attachments (opaque)
    std::vector<VkImage>        gbufferNormalImages;
    std::vector<VkDeviceMemory> gbufferNormalMemorys;
    std::vector<VkImageView>    gbufferNormalImageViews;

    std::vector<VkImage>        gbufferAlbedoImages;
    std::vector<VkDeviceMemory> gbufferAlbedoMemorys;
    std::vector<VkImageView>    gbufferAlbedoImageViews;

    std::vector<VkImage>        gbufferMaterialImages;
    std::vector<VkDeviceMemory> gbufferMaterialMemorys;
    std::vector<VkImageView>    gbufferMaterialImageViews;

    // Baked light (RGB) attachment
    std::vector<VkImage>        gbufferBakedImages;
    std::vector<VkDeviceMemory> gbufferBakedMemorys;
    std::vector<VkImageView>    gbufferBakedImageViews;

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
    std::vector<VkFramebuffer> depthPrepassFramebuffers;
    std::vector<VkFramebuffer> loadDepthFramebuffers;
    std::vector<VkFramebuffer> loadColorDepthFramebuffers;
    std::vector<VkFramebuffer> gbufferFramebuffers;
    std::vector<VkFramebuffer> deferredLightingFramebuffers;
    VkSampler                  sampler{VK_NULL_HANDLE};
    VkSampler                  depthSampler{VK_NULL_HANDLE};
    VkSampler                  hzbSampler{VK_NULL_HANDLE};
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP
