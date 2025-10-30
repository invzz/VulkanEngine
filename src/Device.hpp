#pragma once

#include "Window.hpp"

// std lib headers
#include <string>
#include <vector>

/**
 * @namespace engine
 * @brief Contains core engine classes and Vulkan device management.
 */
namespace engine {

    /**
     * @struct SwapChainSupportDetails
     * @brief Holds swapchain capabilities, formats, and present modes for a physical device.
     */
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    /**
     * @struct QueueFamilyIndices
     * @brief Stores indices for graphics and present queue families.
     */
    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool     graphicsFamilyHasValue = false;
        bool     presentFamilyHasValue  = false;
        bool     isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    /**
     * @class Device
     * @brief Manages Vulkan device, queues, command pool, and related resources.
     */
    class Device
    {
      public:
#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
        const bool enableValidationLayers = true;
#endif
        /**
         * @brief Constructs the Device and initializes Vulkan resources.
         * @param window Reference to the main application window.
         */
        Device(Window& window);

        /**
         * @brief Destructor. Cleans up Vulkan resources and device.
         */
        ~Device();

        // Not copyable or movable
        Device(const Device&)         = delete;
        void operator=(const Device&) = delete;
        Device(Device&&)              = delete;
        Device& operator=(Device&&)   = delete;

        /** @brief Returns the Vulkan command pool handle. */
        VkCommandPool getCommandPool() { return commandPool; }
        /** @brief Returns the Vulkan logical device handle. */
        VkDevice device() { return device_; }
        /** @brief Returns the Vulkan surface handle. */
        VkSurfaceKHR surface() { return surface_; }
        /** @brief Returns the graphics queue handle. */
        VkQueue graphicsQueue() { return graphicsQueue_; }
        /** @brief Returns the present queue handle. */
        VkQueue presentQueue() { return presentQueue_; }

        /**
         * @brief Gets swapchain support details for the current physical device.
         * @return SwapChainSupportDetails struct.
         */
        SwapChainSupportDetails getSwapChainSupport()
        {
            return querySwapChainSupport(physicalDevice);
        }

        /**
         * @brief Finds a suitable memory type for Vulkan resource allocation.
         * @param typeFilter Bitmask of acceptable memory types.
         * @param properties Required memory property flags.
         * @return Index of suitable memory type.
         */
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        /**
         * @brief Finds graphics and present queue families for the current physical device.
         * @return QueueFamilyIndices struct with found indices.
         */
        QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

        /**
         * @brief Finds a supported Vulkan image format from candidates.
         * @param candidates List of VkFormat candidates.
         * @param tiling Desired image tiling.
         * @param features Required format features.
         * @return Supported VkFormat.
         */
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                     VkImageTiling                tiling,
                                     VkFormatFeatureFlags         features);

        // Buffer Helper Functions

        /**
         * @brief Creates a Vulkan buffer and allocates memory for it.
         * @param size Size of the buffer in bytes.
         * @param usage Buffer usage flags.
         * @param properties Memory property flags.
         * @param buffer Reference to buffer handle to be created.
         * @param bufferMemory Reference to memory handle to be allocated.
         */
        void createBuffer(VkDeviceSize          size,
                          VkBufferUsageFlags    usage,
                          VkMemoryPropertyFlags properties,
                          VkBuffer&             buffer,
                          VkDeviceMemory&       bufferMemory);

        /**
         * @brief Begins recording a single-use command buffer.
         * @return Handle to the allocated command buffer.
         */
        VkCommandBuffer beginSingleTimeCommands();

        /**
         * @brief Ends recording and submits a single-use command buffer.
         * @param commandBuffer Command buffer to end and submit.
         */
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        /**
         * @brief Copies data from one Vulkan buffer to another.
         * @param srcBuffer Source buffer handle.
         * @param dstBuffer Destination buffer handle.
         * @param size Number of bytes to copy.
         */
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        /**
         * @brief Copies buffer data to a Vulkan image.
         * @param buffer Source buffer handle.
         * @param image Destination image handle.
         * @param width Image width.
         * @param height Image height.
         * @param layerCount Number of layers in the image.
         */
        void copyBufferToImage(VkBuffer buffer,
                               VkImage  image,
                               uint32_t width,
                               uint32_t height,
                               uint32_t layerCount);

        /**
         * @brief Creates a Vulkan image and allocates memory for it.
         * @param imageInfo Image creation info struct.
         * @param properties Memory property flags.
         * @param image Reference to image handle to be created.
         * @param imageMemory Reference to memory handle to be allocated.
         */
        void createImageWithInfo(const VkImageCreateInfo& imageInfo,
                                 VkMemoryPropertyFlags    properties,
                                 VkImage&                 image,
                                 VkDeviceMemory&          imageMemory);

        /** @brief Properties of the selected Vulkan physical device. */
        VkPhysicalDeviceProperties properties;

      private:
        /**
         * @brief Creates the Vulkan instance and checks required extensions.
         */
        void createInstance();

        /**
         * @brief Sets up the Vulkan debug messenger for validation layer output.
         */
        void setupDebugMessenger();

        /**
         * @brief Creates the Vulkan surface for window presentation.
         */
        void createSurface();

        /**
         * @brief Selects a suitable physical device (GPU) for Vulkan operations.
         */
        void pickPhysicalDevice();

        /**
         * @brief Creates the Vulkan logical device and retrieves graphics/present queues.
         */
        void createLogicalDevice();

        /**
         * @brief Creates a command pool for allocating Vulkan command buffers.
         */
        void createCommandPool();

        // helper functions

        /**
         * @brief Checks if a physical device is suitable for engine requirements.
         * @param device Vulkan physical device handle.
         * @return true if device is suitable, false otherwise.
         */
        bool isDeviceSuitable(VkPhysicalDevice device);

        /**
         * @brief Gets required Vulkan instance extensions for GLFW and validation layers.
         * @return Vector of required extension names.
         */
        std::vector<const char*> getRequiredExtensions();

        /**
         * @brief Checks if requested Vulkan validation layers are available.
         * @return true if all requested layers are available, false otherwise.
         */
        bool checkValidationLayerSupport();

        /**
         * @brief Finds graphics and present queue families for a physical device.
         * @param device Vulkan physical device handle.
         * @return QueueFamilyIndices struct with found indices.
         */
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        /**
         * @brief Populates debug messenger creation info for Vulkan validation layers.
         * @param createInfo Reference to debug messenger creation info struct.
         */
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        /**
         * @brief Checks and prints available and required Vulkan instance extensions.
         */
        void hasGflwRequiredInstanceExtensions();

        /**
         * @brief Checks if a physical device supports required Vulkan device extensions.
         * @param device Vulkan physical device handle.
         * @return true if all required extensions are supported, false otherwise.
         */
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);

        /**
         * @brief Queries swapchain support details for a physical device.
         * @param device Vulkan physical device handle.
         * @return SwapChainSupportDetails struct with capabilities, formats, and present modes.
         */
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkInstance               instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
        Window&                  window;
        VkCommandPool            commandPool;

        VkDevice     device_;
        VkSurfaceKHR surface_;
        VkQueue      graphicsQueue_;
        VkQueue      presentQueue_;

        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };

} // namespace engine