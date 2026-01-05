#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAIN_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAIN_HPP

#include "Engine/Graphics/Device.hpp"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace engine {

  class SwapChain
  {
  public:
    static int maxFramesInFlight() { return 2; }

    SwapChain(Device& deviceRef, VkExtent2D windowExtent);
    SwapChain(Device& deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous);

    ~SwapChain();

    SwapChain(const SwapChain&)            = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    VkFramebuffer            getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
    VkRenderPass             getRenderPass() { return renderPass; }
    VkImageView              getImageView(int index) { return swapChainImageViews[index]; }
    [[nodiscard]] size_t     imageCount() const { return swapChainImages.size(); }
    [[nodiscard]] VkFormat   getSwapChainImageFormat() const { return swapChainImageFormat; }
    [[nodiscard]] VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
    [[nodiscard]] uint32_t   width() const { return swapChainExtent.width; }
    [[nodiscard]] uint32_t   height() const { return swapChainExtent.height; }

    [[nodiscard]] float extentAspectRatio() const { return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height); }

    VkFormat findDepthFormat();

    [[nodiscard]] bool compareSwapFormats(const SwapChain& other) const { return other.swapChainDepthFormat == swapChainDepthFormat && other.swapChainImageFormat == swapChainImageFormat; }

    VkResult acquireNextImage(uint32_t* imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);

  private:
    void Init();
    void createSwapChain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    // Helper functions
    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    [[nodiscard]] VkPresentModeKHR   chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    [[nodiscard]] VkExtent2D         chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    VkFormat   swapChainImageFormat;
    VkFormat   swapChainDepthFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass               renderPass;

    std::vector<VkImage>        depthImages;
    std::vector<VkDeviceMemory> depthImageMemorys;
    std::vector<VkImageView>    depthImageViews;
    std::vector<VkImage>        swapChainImages;
    std::vector<VkImageView>    swapChainImageViews;

    Device&    device;
    VkExtent2D windowExtent;

    VkSwapchainKHR swapChain;

    std::shared_ptr<SwapChain> oldSwapChain;

    // Per-image semaphores for swapchain synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
    std::vector<VkFence>     imagesInFlight;
    size_t                   currentFrame = 0;
    struct PresentIdState
    {
      bool     enabled = false;
      uint64_t next    = 1;
    };
    PresentIdState presentIdState;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAIN_HPP
