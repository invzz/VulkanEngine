#pragma once

#include "DeviceMemory.hpp"
#include "Window.hpp"

// std lib headers
#include <memory>
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
        bool     isComplete() const { return graphicsFamilyHasValue && presentFamilyHasValue; }
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
        explicit Device(Window& window);

        /**
         * @brief Destructor. Cleans up Vulkan resources and device.
         */
        ~Device();

        // Not copyable or movable
        Device(const Device&)            = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&)                 = delete;
        Device& operator=(Device&&)      = delete;

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
        /** @brief Returns true if VK_KHR_present_id was enabled on the logical device. */
        bool supportsPresentId() const { return presentIdSupported_; }

        /**
         * @brief Gets swapchain support details for the current physical device.
         * @return SwapChainSupportDetails struct.
         */
        SwapChainSupportDetails getSwapChainSupport()
        {
            return querySwapChainSupport(physicalDevice);
        }

        // moved memory/buffer helpers into DeviceMemory helper class
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

        /**
         * @brief Copies data from one Vulkan buffer to another.
         * @param srcBuffer Source buffer handle.
         * @param dstBuffer Destination buffer handle.
         * @param size Number of bytes to copy.
         */

        /**
         * @brief Copies buffer data to a Vulkan image.
         * @param buffer Source buffer handle.
         * @param image Destination image handle.
         * @param width Image width.
         * @param height Image height.
         * @param layerCount Number of layers in the image.
         */

        /**
         * @brief Creates a Vulkan image and allocates memory for it.
         * @param imageInfo Image creation info struct.
         * @param properties Memory property flags.
         * @param image Reference to image handle to be created.
         * @param imageMemory Reference to memory handle to be allocated.
         */
        // Accessor that returns the dedicated memory helper which contains
        // buffer/image and single-use command helpers previously on Device.
        DeviceMemory& memory() { return *memory_; }

        /** @brief Returns properties of the selected Vulkan physical device. */
        const VkPhysicalDeviceProperties& getProperties() const { return properties; }

      private:
        bool                     checkValidationLayerSupport() const;
        std::vector<const char*> getRequiredExtensions() const;
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
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;

        /**
         * @brief Checks and prints available and required Vulkan instance extensions.
         */
        void hasGflwRequiredInstanceExtensions() const;

        /**
         * @brief Checks if a physical device supports required Vulkan device extensions.
         * @param device Vulkan physical device handle.
         * @return true if all required extensions are supported, false otherwise.
         */
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

        /**
         * @brief Queries swapchain support details for a physical device.
         * @param device Vulkan physical device handle.
         * @return SwapChainSupportDetails struct with capabilities, formats, and present modes.
         */
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkInstance instance;
        /** @brief Properties of the selected Vulkan physical device. */
        VkPhysicalDeviceProperties properties;
        VkDebugUtilsMessengerEXT   debugMessenger;
        VkPhysicalDevice           physicalDevice = VK_NULL_HANDLE;
        Window&                    window;
        VkCommandPool              commandPool;

        VkDevice                       device_;
        VkSurfaceKHR                   surface_;
        VkQueue                        graphicsQueue_;
        VkQueue                        presentQueue_;
        const std::vector<const char*> validationLayers    = {"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char*> deviceExtensions    = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        bool                           presentIdSupported_ = false;

        // helper object that owns memory/buffer helper operations
        std::unique_ptr<DeviceMemory> memory_;
        friend class DeviceMemory;
    };

} // namespace engine
