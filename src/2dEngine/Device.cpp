#include "2dEngine/Device.hpp"

#include "2dEngine/Exceptions.hpp"
#include "2dEngine/ansi_colors.hpp"
// std headers
#include <algorithm>
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
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
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
                .apiVersion         = VK_API_VERSION_1_1,
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
        std::set<uint32_t>                   uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                       .queueFamilyIndex = queueFamily,
                                                       .queueCount       = 1,
                                                       .pQueuePriorities = &queuePriority};

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {
                .samplerAnisotropy = VK_TRUE,
        };

        std::vector<const char*> enabledExtensions(deviceExtensions.begin(), deviceExtensions.end());

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        const bool presentIdExtensionAvailable =
                std::any_of(availableExtensions.begin(), availableExtensions.end(), [](const VkExtensionProperties& extension) {
                    return std::strcmp(extension.extensionName, VK_KHR_PRESENT_ID_EXTENSION_NAME) == 0;
                });

        static_assert(sizeof(PFN_vkGetPhysicalDeviceFeatures2KHR) == sizeof(PFN_vkVoidFunction), "Vulkan function pointer sizes must match");
        PFN_vkGetPhysicalDeviceFeatures2KHR getFeatures2 = nullptr;
        if (const auto rawGetFeatures2KHR = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"); rawGetFeatures2KHR != nullptr)
        {
            std::memcpy(&getFeatures2, &rawGetFeatures2KHR, sizeof(getFeatures2));
        }
        if (getFeatures2 == nullptr)
        {
            if (const auto rawGetFeatures2 = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"); rawGetFeatures2 != nullptr)
            {
                std::memcpy(&getFeatures2, &rawGetFeatures2, sizeof(getFeatures2));
            }
        }

        VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesQuery = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
        };
        presentIdSupported_ = false;

        if (presentIdExtensionAvailable && getFeatures2 != nullptr)
        {
            VkPhysicalDeviceFeatures2 features2 = {
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                    .pNext = &presentIdFeaturesQuery,
            };
            getFeatures2(physicalDevice, &features2);
            if (presentIdFeaturesQuery.presentId == VK_TRUE)
            {
                presentIdSupported_ = true;
                enabledExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
            }
        }

        VkDeviceCreateInfo createInfo = {
                .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos    = queueCreateInfos.data(),
                .pEnabledFeatures     = &deviceFeatures,
        };

        createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesEnable = {
                .sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
                .pNext     = nullptr,
                .presentId = VK_TRUE,
        };
        if (presentIdSupported_)
        {
            createInfo.pNext = &presentIdFeaturesEnable;
        }

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
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

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
            swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    bool Device::checkValidationLayerSupport() const
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers)
        {
            const bool found = std::any_of(availableLayers.begin(), availableLayers.end(), [layerName](const VkLayerProperties& layerProps) {
                return std::strcmp(layerName, layerProps.layerName) == 0;
            });
            if (!found)
            {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> Device::getRequiredExtensions() const
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void Device::setupDebugMessenger()
    {
        if (!enableValidationLayers)
        {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to set up debug messenger!");
        }
    }

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices{};

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.graphicsFamily         = i;
                indices.graphicsFamilyHasValue = true;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport == VK_TRUE)
            {
                indices.presentFamily         = i;
                indices.presentFamilyHasValue = true;
            }

            if (indices.isComplete())
            {
                break;
            }

            ++i;
        }

        return indices;
    }

    void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const
    {
        createInfo                 = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData       = nullptr;
    }

    void Device::hasGflwRequiredInstanceExtensions() const
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::unordered_set<std::string> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty())
        {
            throw engine::RuntimeException("missing required GLFW instance extensions");
        }
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details{};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

            if (tiling == VK_IMAGE_TILING_LINEAR && (formatProperties.linearTilingFeatures & features) == features)
            {
                return format;
            }

            if (tiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw engine::RuntimeException("failed to find supported format!");
    }

} // namespace engine