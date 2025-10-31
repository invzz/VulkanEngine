#include "Device.hpp"

#include "Exceptions.hpp"
#include "ansi_colors.hpp"
// std headers
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

/**
 * @namespace engine
 * @brief Contains core engine classes and functions.
 */
namespace engine {

    // local callback functions
    static VKAPI_ATTR VkBool32 VKAPI_CALL

    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT             messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* /*pUserData*/)
    {
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        {
            // General message
            std::cerr << "[ " << GREEN "GENERAL" RESET;
        }
        else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        {
            // Validation message
            std::cerr << "[ " << YELLOW "VALIDATION" RESET;
        }
        else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        {
            // Performance message
            std::cerr << "[ " << BLUE "PERFORMANCE" RESET;
        }
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            std::cerr << RED " ERROR" RESET " ] " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cerr << YELLOW " WARNING" RESET " ] " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            std::cerr << BLUE " INFO" RESET " ] " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        {
            std::cerr << CYAN " VERBOSE" RESET " ] " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }
        return VK_FALSE;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance                                instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks*              pAllocator,
                                          VkDebugUtilsMessengerEXT*                 pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance,
                "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance                   instance,
                                       VkDebugUtilsMessengerEXT     debugMessenger,
                                       const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance,
                "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }

    // class member functions
    /**
     * @class Device
     * @brief Manages Vulkan device, queues, and related resources.
     * @param window Reference to the main application window.
     */
    Device::Device(Window& window) : window{window}
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        // initialize memory helper (depends on device_ and commandPool being created)
        memory_ = std::make_unique<DeviceMemory>(*this);
    }

    /**
     * @brief Destructor. Cleans up Vulkan resources and device.
     */
    Device::~Device()
    {
        // ensure helper is destroyed before device/command pool teardown
        memory_.reset();
        vkDestroyCommandPool(device_, commandPool, nullptr);
        vkDestroyDevice(device_, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface_, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    /**
     * @brief Creates the Vulkan instance and checks required extensions.
     * @throws std::runtime_error if validation layers or instance creation fails.
     */
    void Device::createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw engine::RuntimeException("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo = {
                .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName   = "LittleVulkanEngine App",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName        = "No Engine",
                .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion         = VK_API_VERSION_1_0,
        };

        VkInstanceCreateInfo createInfo = {
                .sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appInfo,
        };

        auto extensions                    = getRequiredExtensions();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext             = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to create instance!");
        }

        hasGflwRequiredInstanceExtensions();
    }

    /**
     * @brief Selects a suitable physical device (GPU) for Vulkan operations.
     * @throws std::runtime_error if no suitable device is found.
     */
    void Device::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw engine::RuntimeException("failed to find GPUs with Vulkan support!");
        }
        std::cout << "Device count: " << deviceCount << std::endl;
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            std::cout << "[ " << YELLOW "Checking device" RESET " ] " << device << std::endl;

            if (isDeviceSuitable(device))
            {
                std::cout << "[ " << GREEN "Device suitable" RESET " ] " << device << std::endl;
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            throw engine::RuntimeException("failed to find a suitable GPU!");
        }

        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "physical device: " << properties.deviceName << std::endl;
    }

    /**
     * @brief Creates the Vulkan logical device and retrieves graphics/present queues.
     * @throws std::runtime_error if device creation fails.
     */
    void Device::createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {
                    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = queueFamily,
                    .queueCount       = 1,
                    .pQueuePriorities = &queuePriority};

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {
                .samplerAnisotropy = VK_TRUE,
        };

        VkDeviceCreateInfo createInfo = {
                .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos       = queueCreateInfos.data(),
                .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
                .pEnabledFeatures        = &deviceFeatures,
        };

        // might not really be necessary anymore because device specific validation layers
        // have been deprecated
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to create logical device!");
        }

        vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
    }

    /**
     * @brief Creates a command pool for allocating Vulkan command buffers.
     * @throws std::runtime_error if command pool creation fails.
     */
    void Device::createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex        = queueFamilyIndices.graphicsFamily;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to create command pool!");
        }
    }

    /**
     * @brief Creates the Vulkan surface for window presentation.
     */
    void Device::createSurface()
    {
        window.createWindowSurface(instance, &surface_);
    }

    /**
     * @brief Checks if a physical device is suitable for engine requirements.
     * @param device Vulkan physical device handle.
     * @return true if device is suitable, false otherwise.
     */
    bool Device::isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate                        = !swapChainSupport.formats.empty() &&
                                !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate &&
               supportedFeatures.samplerAnisotropy;
    }

    /**
     * @brief Populates debug messenger creation info for Vulkan validation layers.
     * @param createInfo Reference to debug messenger creation info struct.
     */
    void
    Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const
    {
        createInfo                 = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData       = nullptr; // Optional
    }

    /**
     * @brief Sets up the Vulkan debug messenger for validation layer output.
     * @throws std::runtime_error if setup fails.
     */
    void Device::setupDebugMessenger()
    {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) !=
            VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to set up debug messenger!");
        }
    }

    /**
     * @brief Checks if requested Vulkan validation layers are available.
     * @return true if all requested layers are available, false otherwise.
     */
    bool Device::checkValidationLayerSupport() const
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Gets required Vulkan instance extensions for GLFW and validation layers.
     * @return Vector of required extension names.
     */
    std::vector<const char*> Device::getRequiredExtensions() const
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    /**
     * @brief Checks and prints available and required Vulkan instance extensions.
     * @throws std::runtime_error if a required extension is missing.
     */
    void Device::hasGflwRequiredInstanceExtensions()
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:" << std::endl;
        std::unordered_set<std::string> available;
        for (const auto& extension : extensions)
        {
            std::cout << "\t" << extension.extensionName << std::endl;
            available.emplace(extension.extensionName);
        }

        std::cout << "required extensions:" << std::endl;
        auto                               requiredExtensions = getRequiredExtensions();
        std::set<std::string, std::less<>> requiredSet(requiredExtensions.begin(),
                                                       requiredExtensions.end());
        for (const auto& required : requiredSet)
        {
            std::cout << "\t" << required << std::endl;
            if (available.find(required) == available.end())
            {
                throw engine::RuntimeException("Missing required glfw extension");
            }
        }
    }

    /**
     * @brief Checks if a physical device supports required Vulkan device extensions.
     * @param device Vulkan physical device handle.
     * @return true if all required extensions are supported, false otherwise.
     */
    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device,
                                             nullptr,
                                             &extensionCount,
                                             availableExtensions.data());

        std::set<std::string, std::less<>> requiredExtensions(deviceExtensions.begin(),
                                                              deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    /**
     * @brief Finds graphics and present queue families for a physical device.
     * @param device Vulkan physical device handle.
     * @return QueueFamilyIndices struct with found indices.
     */
    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily         = i;
                indices.graphicsFamilyHasValue = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport)
            {
                indices.presentFamily         = i;
                indices.presentFamilyHasValue = true;
            }
            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    /**
     * @brief Queries swapchain support details for a physical device.
     * @param device Vulkan physical device handle.
     * @return SwapChainSupportDetails struct with capabilities, formats, and present modes.
     */
    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device,
                                                 surface_,
                                                 &formatCount,
                                                 details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                      surface_,
                                                      &presentModeCount,
                                                      details.presentModes.data());
        }
        return details;
    }

    /**
     * @brief Finds a supported Vulkan image format from candidates.
     * @param candidates List of VkFormat candidates.
     * @param tiling Desired image tiling.
     * @param features Required format features.
     * @return Supported VkFormat.
     * @throws std::runtime_error if no supported format is found.
     */
    VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                         VkImageTiling                tiling,
                                         VkFormatFeatureFlags         features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                     (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        throw engine::RuntimeException("failed to find supported format!");
    }

    /**
     * @brief Finds a suitable memory type for Vulkan resource allocation.
     * @param typeFilter Bitmask of acceptable memory types.
     * @param properties Required memory property flags.
     * @return Index of suitable memory type.
     * @throws std::runtime_error if no suitable memory type is found.
     */
    uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                            memoryPropertyFlags) == memoryPropertyFlags)
            {
                return i;
            }
        }

        throw engine::RuntimeException("failed to find suitable memory type!");
    }

    /**
     * @brief Creates a Vulkan buffer and allocates memory for it.
     * @param size Size of the buffer in bytes.
     * @param usage Buffer usage flags.
     * @param properties Memory property flags.
     * @param buffer Reference to buffer handle to be created.
     * @param bufferMemory Reference to memory handle to be allocated.

        {
            throw engine::RuntimeException("failed to create vertex buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex =
                findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

        if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to allocate vertex buffer memory!");
        }

        vkBindBufferMemory(device_, buffer, bufferMemory, 0);
    }

    // The memory/buffer helpers have been moved into DeviceMemory helper class.

} // namespace engine