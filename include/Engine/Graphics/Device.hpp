#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICE_HPP

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"

namespace engine {

  struct SwapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
  };

  struct QueueFamilyIndices
  {
    uint32_t           graphicsFamily;
    uint32_t           presentFamily;
    bool               graphicsFamilyHasValue = false;
    bool               presentFamilyHasValue  = false;
    [[nodiscard]] bool isComplete() const { return graphicsFamilyHasValue && presentFamilyHasValue; }
  };

  class Device
  {
  public:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    // For debugging GPU hangs we enable validation layers unconditionally here.
    const bool enableValidationLayers = true;

    void WaitIdle() { vkDeviceWaitIdle(device_); }

    explicit Device(Window& window);

    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&)                 = delete;
    Device& operator=(Device&&)      = delete;

    VkCommandPool      getCommandPool() { return commandPool; }
    DeviceMemory&      getMemory() { return *memory_; }
    VkDevice           device() { return device_; }
    VkSurfaceKHR       surface() { return surface_; }
    VkQueue            graphicsQueue() { return graphicsQueue_; }
    VkQueue            presentQueue() { return presentQueue_; }
    VkInstance         getInstance() { return instance; }
    [[nodiscard]] bool supportsPresentId() const { return presentIdSupported_; }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    DeviceMemory& memory() { return *memory_; }

    [[nodiscard]] const VkPhysicalDeviceProperties& getProperties() const { return properties; }

    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

    VkCommandBuffer beginSingleTimeCommands();
    void            endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Allocate / free helpers for secondary command buffers.
    // If thread-local command pools are enabled, allocation is done from the
    // calling thread's pool to avoid cross-thread contention. The underlying
    // pool used is tracked so the command buffer can be freed correctly later.
    VkResult allocateSecondaryCommandBuffer(VkCommandBuffer* outCommandBuffer);
    void     freeSecondaryCommandBuffer(VkCommandBuffer commandBuffer);

    // Opt-in thread-local command pools for single-time commands. When enabled
    // a per-thread VkCommandPool is used to avoid creating/destroying pools
    // per-call which can reduce contention and allocation churn on multithreaded workloads.
    void enableThreadLocalCommandPools();
    bool threadLocalCommandPoolsEnabled() const { return threadLocalCommandPools_ != nullptr; }

    // Serialize queue submissions from multiple threads to avoid simultaneous
    // use of a VkQueue object which triggers validation errors.
    VkResult submitGraphics(const VkSubmitInfo* submitInfo, VkFence fence);
    VkResult present(const VkPresentInfoKHR* presentInfo);

    // Deferred destruction: enqueue Vulkan destroys to run after the in-flight fence for a frame.
    // SwapChain drives the current frame index and flushes the queue once its fence is waited.
    void     setCurrentFrameIndex(uint32_t frameIndex);
    uint32_t getCurrentFrameIndex() const { return currentFrameIndex_; }
    void     deferDestroy(std::function<void(VkDevice)> fn);
    void     flushDeferred(uint32_t frameIndex);
    void     flushAllDeferred();

    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;

  private:
    [[nodiscard]] bool                     checkValidationLayerSupport() const;
    [[nodiscard]] std::vector<const char*> getRequiredExtensions() const;
    void                                   createInstance();
    void                                   setupDebugMessenger();
    void                                   createSurface();
    void                                   pickPhysicalDevice();
    void                                   createLogicalDevice();
    void                                   createCommandPool();

    bool                    isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice device);
    static void             populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static bool             hasGlfwRequiredInstanceExtensions();
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkInstance                 instance = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    // Debug messenger is created explicitly via `setupDebugMessenger()` so the
    // Device retains an owning handle and can destroy it deterministically in
    // shutdown. Do NOT attach a DebugUtils create info to the instance `pNext` —
    // that pattern hides ownership and makes safe teardown ordering harder.
    std::unique_ptr<class DebugMessenger> debugMessenger;
    // Optional thread-local command pool manager for single-time command paths.
    std::unique_ptr<class ThreadLocalCommandPool>                              threadLocalCommandPools_;
    VkPhysicalDevice                                                           physicalDevice = VK_NULL_HANDLE;
    Window&                                                                    window;
    VkCommandPool                                                              commandPool         = VK_NULL_HANDLE;
    VkDevice                                                                   device_             = VK_NULL_HANDLE;
    VkSurfaceKHR                                                               surface_            = VK_NULL_HANDLE;
    VkQueue                                                                    graphicsQueue_      = VK_NULL_HANDLE;
    VkQueue                                                                    presentQueue_       = VK_NULL_HANDLE;
    const std::vector<const char*>                                             validationLayers    = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*>                                             deviceExtensions    = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool                                                                       presentIdSupported_ = false;
    std::unique_ptr<DeviceMemory>                                              memory_;
    uint32_t                                                                   currentFrameIndex_ = 0;
    std::array<std::vector<std::function<void(VkDevice)>>, kMaxFramesInFlight> deferredDestroy_;

    // Mapping from temporary command buffers to the command pool that owns them. Used
    // for single-time commands when executed concurrently on worker threads.
    mutable std::mutex                                         singleCmdMutex;
    mutable std::unordered_map<VkCommandBuffer, VkCommandPool> cmdBufferToPoolMap_;
    VkFence                                                    singleTimeFence_ = VK_NULL_HANDLE; // Reusable fence for single-time commands

    // Serialize queue submissions
    mutable std::mutex queueSubmitMutex_;

    friend class DeviceMemory;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICE_HPP
