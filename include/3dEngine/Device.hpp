#pragma once

#include <memory>
#include <string>
#include <vector>

#include "DeviceMemory.hpp"
#include "Window.hpp"

namespace engine {

  struct SwapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
  };

  struct QueueFamilyIndices
  {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    bool     graphicsFamilyHasValue = false;
    bool     presentFamilyHasValue  = false;
    bool     isComplete() const { return graphicsFamilyHasValue && presentFamilyHasValue; }
  };

  class Device
  {
  public:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    void WaitIdle() { vkDeviceWaitIdle(device_); }

    explicit Device(Window& window);

    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&)                 = delete;
    Device& operator=(Device&&)      = delete;

    VkCommandPool getCommandPool() { return commandPool; }
    DeviceMemory& getMemory() { return *memory_; }
    VkDevice      device() { return device_; }
    VkSurfaceKHR  surface() { return surface_; }
    VkQueue       graphicsQueue() { return graphicsQueue_; }
    VkQueue       presentQueue() { return presentQueue_; }
    bool          supportsPresentId() const { return presentIdSupported_; }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    DeviceMemory& memory() { return *memory_; }

    const VkPhysicalDeviceProperties& getProperties() const { return properties; }

  private:
    bool                     checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredExtensions() const;
    void                     createInstance();
    void                     setupDebugMessenger();
    void                     createSurface();
    void                     pickPhysicalDevice();
    void                     createLogicalDevice();
    void                     createCommandPool();

    bool                    isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice device);
    void                    populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
    void                    hasGflwRequiredInstanceExtensions() const;
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkInstance                     instance;
    VkPhysicalDeviceProperties     properties;
    VkDebugUtilsMessengerEXT       debugMessenger;
    VkPhysicalDevice               physicalDevice = VK_NULL_HANDLE;
    Window&                        window;
    VkCommandPool                  commandPool;
    VkDevice                       device_;
    VkSurfaceKHR                   surface_;
    VkQueue                        graphicsQueue_;
    VkQueue                        presentQueue_;
    const std::vector<const char*> validationLayers    = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> deviceExtensions    = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool                           presentIdSupported_ = false;
    std::unique_ptr<DeviceMemory>  memory_;
    friend class DeviceMemory;
  };

} // namespace engine
