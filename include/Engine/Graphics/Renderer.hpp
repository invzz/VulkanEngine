#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP

#include <assert.h>
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameBuffer.hpp"
#include "Engine/Graphics/HZBGenerator.hpp"
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
        void generateDepthPyramid(VkCommandBuffer commandBuffer);
        void copyOffscreenColorToSceneColor(VkCommandBuffer commandBuffer);

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
        [[nodiscard]] VkDescriptorImageInfo getHzbImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getSceneColorImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferNormalImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferAlbedoImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferMaterialImageInfo(int index) const;
        [[nodiscard]] VkDescriptorImageInfo getGbufferBakedImageInfo(int index) const;

        // Expose offscreen color image handle for readback in tests
        [[nodiscard]] VkImage getOffscreenColorImage(int index) const {
            return offscreenFrameBuffer->getColorImage(index);
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
        [[nodiscard]] VkExtent2D getSwapChainExtent() const {
            return swapChain->getSwapChainExtent();
        }

       private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        void createOffscreenResources();
        void createHZBPipeline();

        Window&                        window;
        Device&                        device;
        HZBGenerator                   hzbGenerator;
        SwapChainRecreationCoordinator swapChainRecreationCoordinator;
        std::unique_ptr<SwapChain>     swapChain;
        std::vector<VkCommandBuffer>   commandBuffers;

        std::unique_ptr<FrameBuffer> offscreenFrameBuffer;

        uint32_t currentImageIndex{0};
        // keep track of frame index for syncing [0, maxFramesInFlight]
        int  currentFrameIndex{0};
        bool isFrameStarted{false};
        bool swapChainRecreated{false};

        // Tracks whether the swapchain image was rendered to this frame. If false,
        // we'll insert a no-op transition to PRESENT_SRC prior to presenting to
        // avoid leaving swapchain images in VK_IMAGE_LAYOUT_UNDEFINED.
        bool usedSwapchainThisFrame{false};

       public:
        // Helper for diagnostics
        [[nodiscard]] uint64_t getPendingResizeTimeNs() const {
            return swapChainRecreationCoordinator.getPendingResizeTimeNs();
        }
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
