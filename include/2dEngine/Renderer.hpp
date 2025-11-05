#pragma once

#include <assert.h>

#include <memory>

#include "Device.hpp"
#include "SwapChain.hpp"
#include "Window.hpp"

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

    // Accessors
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
    bool         isFrameInProgress() const { return isFrameStarted; }

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

  private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    Window&                      window;
    Device&                      device;
    std::unique_ptr<SwapChain>   swapChain;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t                     currentImageIndex{0};
    // keep track of frame index for syncing [0, maxFramesInFlight]
    int  currentFrameIndex{0};
    bool isFrameStarted{false};
  };

} // namespace engine
