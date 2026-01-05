#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP

#include <assert.h>

#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameBuffer.hpp"
#include "Engine/Graphics/HZBGenerator.hpp"
#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

  class Renderer
  {
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
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer) const;
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer);
    void beginOffscreenDepthPrepassRenderPass(VkCommandBuffer commandBuffer);
    void beginOffscreenRenderPassLoadDepth(VkCommandBuffer commandBuffer);
    void beginOffscreenRenderPassLoadColorDepth(VkCommandBuffer commandBuffer);
    void beginGbufferRenderPass(VkCommandBuffer commandBuffer);
    void beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer);
    void endOffscreenRenderPass(VkCommandBuffer commandBuffer) const;
    void generateOffscreenMipmaps(VkCommandBuffer commandBuffer);
    void generateDepthPyramid(VkCommandBuffer commandBuffer);
    void copyOffscreenColorToSceneColor(VkCommandBuffer commandBuffer);

    // Accessors
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
    VkRenderPass getOffscreenRenderPass() const { return offscreenFrameBuffer->getRenderPass(); }
    VkRenderPass getOffscreenDepthPrepassRenderPass() const { return offscreenFrameBuffer->getDepthPrepassRenderPass(); }
    VkRenderPass getOffscreenRenderPassLoadDepth() const { return offscreenFrameBuffer->getRenderPassLoadDepth(); }
    VkRenderPass getOffscreenRenderPassLoadColorDepth() const { return offscreenFrameBuffer->getRenderPassLoadColorDepth(); }
    VkRenderPass getGbufferRenderPass() const { return offscreenFrameBuffer->getGbufferRenderPass(); }
    VkRenderPass getDeferredLightingRenderPass() const { return offscreenFrameBuffer->getDeferredLightingRenderPass(); }

    VkDescriptorImageInfo getOffscreenImageInfo(int index) const;
    VkDescriptorImageInfo getDepthImageInfo(int index) const;
    VkDescriptorImageInfo getHzbImageInfo(int index) const;
    VkDescriptorImageInfo getSceneColorImageInfo(int index) const;
    VkDescriptorImageInfo getGbufferNormalImageInfo(int index) const;
    VkDescriptorImageInfo getGbufferAlbedoImageInfo(int index) const;
    VkDescriptorImageInfo getGbufferMaterialImageInfo(int index) const;

    bool isFrameInProgress() const { return isFrameStarted; }
    bool wasSwapChainRecreated() const { return swapChainRecreated; }

    VkCommandBuffer getCurrentCommandBuffer() const
    {
      assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
      return commandBuffers[currentFrameIndex];
    }

    int getFrameIndex() const
    {
      assert(isFrameStarted && "Cannot get frame index when frame not in progress");
      return currentFrameIndex;
    }

    float      getAspectRatio() const { return swapChain->extentAspectRatio(); }
    VkExtent2D getSwapChainExtent() const { return swapChain->getSwapChainExtent(); }

  private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();
    void createOffscreenResources();
    void createHZBPipeline();

    Window&                      window;
    Device&                      device;
    HZBGenerator                 hzbGenerator;
    std::unique_ptr<SwapChain>   swapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    std::unique_ptr<FrameBuffer> offscreenFrameBuffer;

    uint32_t currentImageIndex{0};
    // keep track of frame index for syncing [0, maxFramesInFlight]
    int  currentFrameIndex{0};
    bool isFrameStarted{false};
    bool swapChainRecreated{false};
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERER_HPP
