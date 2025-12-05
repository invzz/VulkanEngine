#pragma once

#include <assert.h>

#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameBuffer.hpp"
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
    void endOffscreenRenderPass(VkCommandBuffer commandBuffer) const;
    void generateOffscreenMipmaps(VkCommandBuffer commandBuffer);
    void generateDepthPyramid(VkCommandBuffer commandBuffer);

    // Accessors
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
    VkRenderPass getOffscreenRenderPass() const { return offscreenFrameBuffer->getRenderPass(); }

    VkDescriptorImageInfo getOffscreenImageInfo(int index) const;
    VkDescriptorImageInfo getDepthImageInfo(int index) const;

    bool isFrameInProgress() const { return isFrameStarted; }

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
    std::unique_ptr<SwapChain>   swapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    std::unique_ptr<FrameBuffer> offscreenFrameBuffer;

    // HZB Generation Resources
    VkPipelineLayout      hzbPipelineLayout{VK_NULL_HANDLE};
    VkPipeline            hzbPipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout hzbSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool      hzbDescriptorPool{VK_NULL_HANDLE};
    // Sets for each frame and each mip transition
    // Outer: Frame, Inner: Mip Level
    std::vector<std::vector<VkDescriptorSet>> hzbDescriptorSets;

    uint32_t currentImageIndex{0};
    // keep track of frame index for syncing [0, maxFramesInFlight]
    int  currentFrameIndex{0};
    bool isFrameStarted{false};
  };

} // namespace engine
