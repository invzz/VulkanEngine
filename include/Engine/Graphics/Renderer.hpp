#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP

#include <assert.h>
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameBuffer.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Graphics/SwapChainRecreationCoordinator.hpp"

namespace engine {

    class Renderer {
       public:
        Renderer(Window& window, Device& device);
        ~Renderer();
        // delete copy operations
        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Frame rendering helpers
        VkCommandBuffer beginFrame();
        void            endFrame();

        // Render pass helpers
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void beginOffscreenRenderPass(VkCommandBuffer commandBuffer);
        void beginOffscreenDepthPrepassRenderPass(VkCommandBuffer commandBuffer);
        void beginOffscreenRenderPassLoadDepth(VkCommandBuffer commandBuffer);
        void beginOffscreenRenderPassLoadColorDepth(VkCommandBuffer commandBuffer);
        void beginGbufferRenderPass(VkCommandBuffer commandBuffer, bool allowSecondaryCommandBuffers = false);
        void beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer);
        void endOffscreenRenderPass(VkCommandBuffer commandBuffer) const;
        void generateOffscreenMipmaps(VkCommandBuffer commandBuffer);
        void copyOffscreenColorToSceneColor(VkCommandBuffer commandBuffer);
        void transitionDepthToShaderReadOnly(VkCommandBuffer commandBuffer);

        // Accessors
        [[nodiscard]] VkRenderPass getSwapChainRenderPass() const {
            return swapChain->getRenderPass();
        }
        [[nodiscard]] VkRenderPass getOffscreenRenderPass() const {
            return offscreenFrameBuffer->getRenderPass();
        }
        [[nodiscard]] VkRenderPass getOffscreenDepthPrepassRenderPass() const {
            return offscreenFrameBuffer->getDepthPrepassRenderPass();
        }
        [[nodiscard]] VkRenderPass getOffscreenRenderPassLoadDepth() const {
            return offscreenFrameBuffer->getRenderPassLoadDepth();
        }
        [[nodiscard]] VkRenderPass getOffscreenRenderPassLoadColorDepth() const {
            return offscreenFrameBuffer->getRenderPassLoadColorDepth();
        }
        [[nodiscard]] VkRenderPass getGbufferRenderPass() const {
            return offscreenFrameBuffer->getGbufferRenderPass();
        }
        [[nodiscard]] VkRenderPass getDeferredLightingRenderPass() const {
            return offscreenFrameBuffer->getDeferredLightingRenderPass();
        }

        [[nodiscard]] VkDescriptorImageInfo getOffscreenImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getDepthImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getSceneColorImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferNormalImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferAlbedoImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferMaterialImageInfo(int index) const;

        // Expose offscreen color image handle for readback in tests
        [[nodiscard]] VkImage getOffscreenColorImage(int index) const {
            return offscreenFrameBuffer->getColorImage(index);
        }
        [[nodiscard]] VkImageView getOffscreenColorImageView(int index) const {
            return offscreenFrameBuffer->getColorImageView(index);
        }
        [[nodiscard]] VkSampler getOffscreenColorSampler(int index) const {
            return offscreenFrameBuffer->getColorSampler(index);
        }

        /** Resize the offscreen framebuffer to match viewport panel size. */
        void resizeOffscreenFramebuffer(VkExtent2D extent);

        /**
         * @brief Recreate offscreen framebuffer after swapchain recreation.
         * Must be called in the same frame as ImGui texture re-registration
         * so descriptors don't point to destroyed images.
         */
        void recreateOffscreenFramebuffer();

        [[nodiscard]] VkExtent2D getOffscreenExtent() const {
            return offscreenExtent_;
        }

        /** Transition offscreen color from COLOR_ATTACHMENT_OPTIMAL to
         * SHADER_READ_ONLY_OPTIMAL so ImGui can sample it. */
        void transitionColorToShaderReadOnly(VkCommandBuffer commandBuffer);

        [[nodiscard]] VkImage getOffscreenDepthImage(int index) const {
            return offscreenFrameBuffer->getDepthImage(index);
        }

        [[nodiscard]] bool isFrameInProgress() const {
            return isFrameStarted;
        }
        [[nodiscard]] bool wasSwapChainRecreated() const {
            return swapChainRecreated;
        }

        [[nodiscard]] VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }

        [[nodiscard]] int getFrameIndex() const {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            return currentFrameIndex;
        }

        [[nodiscard]] float getAspectRatio() const {
            return swapChain->extentAspectRatio();
        }
        [[nodiscard]] VkFormat getSwapChainFormat() const {
            return swapChain->getSwapChainImageFormat();
        }

        [[nodiscard]] VkExtent2D getSwapChainExtent() const {
            return swapChain->getSwapChainExtent();
        }

        [[nodiscard]] VkFramebuffer getSwapChainFramebuffer(int index) const {
            return swapChain->getFrameBuffer(index);
        }

        /**
         * @brief Set whether to skip clearing the swap chain in the next render pass.
         * Used when the viewport has already been rendered to the swap chain.
         */
        void setSkipClearSwapChain(bool skip) {
            skipClear_ = skip;
        }

        [[nodiscard]] bool getSkipClearSwapChain() const {
            return skipClear_;
        }

       private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        void createOffscreenResources();

        Window&                        window;
        Device&                        device;
        SwapChainRecreationCoordinator swapChainRecreationCoordinator;
        std::unique_ptr<SwapChain>     swapChain;
        std::vector<VkCommandBuffer>   commandBuffers;

        std::unique_ptr<FrameBuffer> offscreenFrameBuffer;
        VkExtent2D                   offscreenExtent_{0, 0};  // tracks offscreen FB size independently

        uint32_t currentImageIndex{0};
        // keep track of frame index for syncing [0, maxFramesInFlight]
        int  currentFrameIndex{0};
        bool isFrameStarted{false};
        bool swapChainRecreated{false};

        // Tracks whether the swapchain image was rendered to this frame. If false,
        // we'll insert a no-op transition to PRESENT_SRC prior to presenting to
        // avoid leaving swapchain images in VK_IMAGE_LAYOUT_UNDEFINED.
        bool usedSwapchainThisFrame{false};

        // When true, skip clearing the swap chain in beginSwapChainRenderPass.
        // Used when the viewport has already been rendered to the swap chain.
        bool skipClear_{false};

       public:
        // Helper for diagnostics
        [[nodiscard]] uint64_t getPendingResizeTimeNs() const {
            return swapChainRecreationCoordinator.getPendingResizeTimeNs();
        }
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
