#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP

#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/RenderTarget.hpp"

namespace engine {

    class FrameBuffer {
       public:
        struct Attachment {
            VkFormat          format;
            VkImageUsageFlags usage;
            VkImageLayout     finalLayout;
        };

        FrameBuffer(Device& device, VkExtent2D extent, uint32_t frameCount, bool useMipmaps = false);
        ~FrameBuffer();

        FrameBuffer(const FrameBuffer&)            = delete;
        FrameBuffer& operator=(const FrameBuffer&) = delete;

        void resize(VkExtent2D newExtent);

        [[nodiscard]] VkRenderPass getRenderPass() const {
            return renderPass;
        }
        [[nodiscard]] VkRenderPass getDepthPrepassRenderPass() const {
            return depthPrepassRenderPass;
        }
        [[nodiscard]] VkRenderPass getRenderPassLoadDepth() const {
            return renderPassLoadDepth;
        }
        [[nodiscard]] VkRenderPass getRenderPassLoadColorDepth() const {
            return renderPassLoadColorDepth;
        }
        [[nodiscard]] VkRenderPass getGbufferRenderPass() const {
            return gbufferRenderPass;
        }
        [[nodiscard]] VkRenderPass getDeferredLightingRenderPass() const {
            return deferredLightingRenderPass;
        }
        [[nodiscard]] uint32_t getMipLevels() const {
            return mipLevels;
        }
        [[nodiscard]] VkDescriptorImageInfo getDescriptorImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getSceneColorDescriptorImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferNormalImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferAlbedoImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferMaterialImageInfo(int index) const;

        void        beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
        void        beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
        void        beginRenderPassLoadDepth(VkCommandBuffer commandBuffer, int frameIndex);
        void        beginRenderPassLoadColorDepth(VkCommandBuffer commandBuffer, int frameIndex);
        void        beginGbufferRenderPass(VkCommandBuffer commandBuffer, int frameIndex, bool allowSecondaryCommandBuffers = false);
        void        beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer, int frameIndex);
        static void endRenderPass(VkCommandBuffer commandBuffer);
        void        generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex);
        void        generateSceneColorMipmaps(VkCommandBuffer commandBuffer, int frameIndex);

        [[nodiscard]] float getAspectRatio() const {
            return static_cast<float>(extent.width) / static_cast<float>(extent.height);
        }

        [[nodiscard]] VkImageView getDepthImageView(int frameIndex) const {
            return depthTargets[frameIndex].getView();
        }
        [[nodiscard]] VkSampler getDepthSampler() const {
            return depthTargets.empty() ? VK_NULL_HANDLE : depthTargets.front().getSampler();
        }
        [[nodiscard]] VkImage getDepthImage(int frameIndex) const {
            return depthTargets[frameIndex].getImage();
        }

        // Offscreen color image (render target)
        [[nodiscard]] VkImage getColorImage(int frameIndex) const {
            return colorTargets[frameIndex].getImage();
        }

        // G-buffer attachments
        [[nodiscard]] VkImageView getGbufferNormalImageView(int frameIndex) const {
            return gbufferNormalTargets[frameIndex].getView();
        }
        [[nodiscard]] VkImageView getGbufferAlbedoImageView(int frameIndex) const {
            return gbufferAlbedoTargets[frameIndex].getView();
        }
        [[nodiscard]] VkImageView getGbufferMaterialImageView(int frameIndex) const {
            return gbufferMaterialTargets[frameIndex].getView();
        }

        // Scene color copy (for transmission refraction sampling)
        [[nodiscard]] VkImageView getSceneColorImageView(int frameIndex) const {
            return sceneColorTargets[frameIndex].getView();
        }
        [[nodiscard]] VkImage getSceneColorImage(int frameIndex) const {
            return sceneColorTargets[frameIndex].getImage();
        }

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
        std::vector<RenderTarget> colorTargets;

        // Scene color copy (sampled in transmission pass)
        std::vector<RenderTarget> sceneColorTargets;

        // G-buffer attachments (opaque)
        std::vector<RenderTarget> gbufferNormalTargets;

        std::vector<RenderTarget> gbufferAlbedoTargets;

        std::vector<RenderTarget> gbufferMaterialTargets;

        // Depth attachment
        std::vector<RenderTarget> depthTargets;

        std::vector<VkFramebuffer> framebuffers;
        std::vector<VkFramebuffer> depthPrepassFramebuffers;
        std::vector<VkFramebuffer> loadDepthFramebuffers;
        std::vector<VkFramebuffer> loadColorDepthFramebuffers;
        std::vector<VkFramebuffer> gbufferFramebuffers;
        std::vector<VkFramebuffer> deferredLightingFramebuffers;
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEBUFFER_HPP
