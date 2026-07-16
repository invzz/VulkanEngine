#include "Engine/Graphics/Device.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/DebugMessenger.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/ExtensionHelpers.hpp"
#include "Engine/Graphics/ThreadLocalCommandPool.hpp"

#include "GLFW/glfw3.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
namespace {
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                                                 messageType,
        const VkDebugUtilsMessengerCallbackDataEXT*                                     pCallbackData,
        void* /*pUserData*/) {
        if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u) {
            engine::Logger::debug(engine::LogChannel::Render, "[GENERAL] ", pCallbackData->pMessage);
        } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u) {
            engine::Logger::debug(engine::LogChannel::Render, "[VALIDATION] ", pCallbackData->pMessage);
        } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
            engine::Logger::debug(engine::LogChannel::Render, "[PERFORMANCE] ", pCallbackData->pMessage);
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
            engine::Logger::error(engine::LogChannel::Render, "[Vulkan] ERROR ", pCallbackData->pMessage);
            return VK_FALSE;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
            engine::Logger::warn(engine::LogChannel::Render, "[Vulkan] WARNING ", pCallbackData->pMessage);
            return VK_FALSE;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0) {
            engine::Logger::info(engine::LogChannel::Render, "[Vulkan] INFO ", pCallbackData->pMessage);
            return VK_FALSE;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0) {
            engine::Logger::debug(engine::LogChannel::Render, "[Vulkan] VERBOSE ", pCallbackData->pMessage);
            return VK_FALSE;
        }
        return VK_FALSE;
    }
}  // namespace
/**
 * @namespace engine
 * @brief Contains core engine classes and functions.
 */
namespace engine {
    std::atomic<int> Device::validationLayersOverride_{-1};
    void             Device::setValidationLayersEnabledOverride(bool enabled) {
        validationLayersOverride_.store(enabled ? 1 : 0, std::memory_order_relaxed);
    }
    void Device::clearValidationLayersEnabledOverride() {
        validationLayersOverride_.store(-1, std::memory_order_relaxed);
    }
    bool Device::resolveValidationLayersEnabled() {
        const int overrideValue = validationLayersOverride_.load(std::memory_order_relaxed);
        if (overrideValue == 0) {
            return false;
        }
        if (overrideValue == 1) {
            return true;
        }
        return kBuildValidationLayersEnabled;
    }
    void Device::setCurrentFrameIndex(uint32_t frameIndex) {
        currentFrameIndex_ = frameIndex % kMaxFramesInFlight;
    }
    void Device::deferDestroy(std::function<void(VkDevice)> fn) {
        if (!fn) {
            return;
        }
        std::lock_guard<std::mutex> lock(deferredDestroyMutex_);
        deferredDestroy_[currentFrameIndex_].push_back(std::move(fn));
    }
    void Device::flushDeferred(uint32_t frameIndex) {
        std::vector<std::function<void(VkDevice)>> bucket;
        {
            std::lock_guard<std::mutex> lock(deferredDestroyMutex_);
            bucket.swap(deferredDestroy_[frameIndex % kMaxFramesInFlight]);
        }
        for (auto& fn : bucket) {
            if (fn) {
                fn(device_);
            }
        }
    }
    void Device::flushAllDeferred() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            flushDeferred(i);
        }
    }
    size_t Device::SamplerCacheKeyHash::operator()(const SamplerCacheKey& key) const {
        auto hashCombine = [](size_t seed, uint32_t value) {
            return seed ^ (static_cast<size_t>(value) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U));
        };
        size_t seed = 0;
        seed        = hashCombine(seed, key.magFilter);
        seed        = hashCombine(seed, key.minFilter);
        seed        = hashCombine(seed, key.mipmapMode);
        seed        = hashCombine(seed, key.addressModeU);
        seed        = hashCombine(seed, key.addressModeV);
        seed        = hashCombine(seed, key.addressModeW);
        seed        = hashCombine(seed, key.anisotropyEnable);
        seed        = hashCombine(seed, key.maxAnisotropyBits);
        seed        = hashCombine(seed, key.compareEnable);
        seed        = hashCombine(seed, key.compareOp);
        seed        = hashCombine(seed, key.minLodBits);
        seed        = hashCombine(seed, key.maxLodBits);
        seed        = hashCombine(seed, key.mipLodBiasBits);
        seed        = hashCombine(seed, key.borderColor);
        seed        = hashCombine(seed, key.unnormalizedCoordinates);
        return seed;
    }
    Device::SamplerCacheKey Device::makeSamplerCacheKey(const VkSamplerCreateInfo& createInfo) const {
        return SamplerCacheKey{
            static_cast<uint32_t>(createInfo.magFilter),
            static_cast<uint32_t>(createInfo.minFilter),
            static_cast<uint32_t>(createInfo.mipmapMode),
            static_cast<uint32_t>(createInfo.addressModeU),
            static_cast<uint32_t>(createInfo.addressModeV),
            static_cast<uint32_t>(createInfo.addressModeW),
            static_cast<uint32_t>(createInfo.anisotropyEnable),
            std::bit_cast<uint32_t>(createInfo.maxAnisotropy),
            static_cast<uint32_t>(createInfo.compareEnable),
            static_cast<uint32_t>(createInfo.compareOp),
            std::bit_cast<uint32_t>(createInfo.minLod),
            std::bit_cast<uint32_t>(createInfo.maxLod),
            std::bit_cast<uint32_t>(createInfo.mipLodBias),
            static_cast<uint32_t>(createInfo.borderColor),
            static_cast<uint32_t>(createInfo.unnormalizedCoordinates),
        };
    }
    VkSampler Device::getOrCreateSampler(const VkSamplerCreateInfo& createInfo) {
        std::lock_guard<std::mutex> lk(samplerCacheMutex_);
        SamplerCacheKey const       key = makeSamplerCacheKey(createInfo);
        auto const                  it  = samplerCache_.find(key);
        if (it != samplerCache_.end()) {
            samplerCacheHits_++;
            return it->second;
        }
        VkSampler sampler = VK_NULL_HANDLE;
        if (vkCreateSampler(device_, &createInfo, nullptr, &sampler) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        samplerCache_[key] = sampler;
        samplerCacheMisses_++;
        return sampler;
    }
    Device::SamplerCacheStats Device::getSamplerCacheStats() const {
        std::lock_guard<std::mutex> lk(samplerCacheMutex_);
        SamplerCacheStats           stats{};
        stats.cacheHits      = samplerCacheHits_;
        stats.cacheMisses    = samplerCacheMisses_;
        stats.cachedSamplers = static_cast<uint64_t>(samplerCache_.size());
        return stats;
    }
    void Device::destroySamplerCache() {
        std::lock_guard<std::mutex> lk(samplerCacheMutex_);
        if (device_ == VK_NULL_HANDLE) {
            samplerCache_.clear();
            samplerCacheHits_   = 0;
            samplerCacheMisses_ = 0;
            return;
        }
        for (auto& [_, sampler] : samplerCache_) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device_, sampler, nullptr);
            }
        }
        samplerCache_.clear();
    }
    /**
 * @class Device
 * @brief Manages Vulkan device, queues, and related resources.
 * @param window Reference to the main application window.
 */
    Device::Device(Window& window) : enableValidationLayers(resolveValidationLayersEnabled()), window{window} {
        engine::Logger::info(engine::LogChannel::General, "[Device] Validation layers ", (enableValidationLayers ? "enabled" : "disabled"), " (build_default=", (kBuildValidationLayersEnabled ? "on" : "off"), ")");
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        memory_ = std::make_unique<DeviceMemory>(*this);
    }
    /**
 * @brief Destructor. Cleans up Vulkan resources and device.
 */
    Device::~Device() {
        try {
            if (device_ != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device_);
            }
            try {
                if (device_ != VK_NULL_HANDLE) {
                    flushAllDeferred();
                }
            } catch (const std::exception& e) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] flushAllDeferred threw: ", e.what());
            } catch (...) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] flushAllDeferred threw unknown exception");
            }
            try {
                destroySamplerCache();
            } catch (const std::exception& e) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] destroySamplerCache threw: ", e.what());
            } catch (...) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] destroySamplerCache threw unknown exception");
            }
            try {
                memory_.reset();
            } catch (const std::exception& e) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] DeviceMemory destructor threw: ", e.what());
            } catch (...) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] DeviceMemory destructor threw unknown exception");
            }
            try {
                if (threadLocalCommandPools_) {
                    threadLocalCommandPools_->destroyAll();
                    threadLocalCommandPools_.reset();
                }
            } catch (const std::exception& e) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] destroyAll threadLocalCommandPools threw: ", e.what());
            } catch (...) {
                engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] destroyAll threadLocalCommandPools threw unknown exception");
            }
            if (device_ != VK_NULL_HANDLE && singleTimeFence_ != VK_NULL_HANDLE) {
                vkDestroyFence(device_, singleTimeFence_, nullptr);
                singleTimeFence_ = VK_NULL_HANDLE;
            }
            if (device_ != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device_, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }
            if (device_ != VK_NULL_HANDLE) {
                vkDestroyDevice(device_, nullptr);
                device_ = VK_NULL_HANDLE;
            }
            if (instance != VK_NULL_HANDLE) {
                if (enableValidationLayers && debugMessenger) {
                    try {
                        debugMessenger.reset();
                    } catch (const std::exception& e) {
                        engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] DebugMessenger destructor threw: ", e.what());
                    } catch (...) {
                        engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] DebugMessenger destructor threw unknown exception");
                    }
                }
                if (surface_ != VK_NULL_HANDLE) {
                    vkDestroySurfaceKHR(instance, surface_, nullptr);
                    surface_ = VK_NULL_HANDLE;
                }
                vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] Exception during shutdown: ", e.what());
        } catch (...) {
            engine::Logger::error(engine::LogChannel::Render, "[Device::~Device] Unknown exception during shutdown");
        }
    }
    /**
 * @brief Creates the Vulkan instance and checks required extensions.
 * @throws std::runtime_error if validation layers or instance creation
 * fails.
 */
    void Device::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw engine::RuntimeException("validation layers requested, but not available!");
        }
        VkApplicationInfo const appInfo = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = "LittleVulkanEngine App",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3,
        };
        VkInstanceCreateInfo createInfo = {
            .sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
        };
        auto extensions                    = getRequiredExtensions();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        if (enableValidationLayers) {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            createInfo.pNext               = nullptr;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext             = nullptr;
        }
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create instance!");
        }
        if (!hasGlfwRequiredInstanceExtensions()) {
            throw engine::RuntimeException("missing required GLFW instance extensions");
        }
    }
    /**
 * @brief Selects a suitable physical device (GPU) for Vulkan operations.
 * @throws std::runtime_error if no suitable device is found.
 */
    void Device::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw engine::RuntimeException("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        VkPhysicalDevice           bestDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties best       = {};
        for (const auto& device : devices) {
            if (!isDeviceSuitable(device))
                continue;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            if (bestDevice == VK_NULL_HANDLE || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && best.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                props.deviceType == best.deviceType && props.limits.maxImageDimension2D > best.limits.maxImageDimension2D) {
                bestDevice = device;
                best       = props;
            }
        }
        if (bestDevice == VK_NULL_HANDLE) {
            throw engine::RuntimeException("failed to find a suitable GPU!");
        }
        physicalDevice = bestDevice;
        properties     = best;
        engine::Logger::info(engine::LogChannel::General, "physical device: ", properties.deviceName);
    }
    void Device::createLogicalDevice() {
        QueueFamilyIndices const             indices = findQueueFamilies(physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> const             uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};
        float const                          queuePriority       = 1.0f;
        for (uint32_t const queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo const queueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &queuePriority};
            queueCreateInfos.push_back(queueCreateInfo);
        }
        VkPhysicalDeviceFeatures const deviceFeatures = {
            .samplerAnisotropy = VK_TRUE,
            .shaderInt64       = VK_TRUE,
        };
        std::vector<const char*> enabledExtensions(deviceExtensions.begin(), deviceExtensions.end());
        enabledExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        const auto availableExtensions         = engine::enumerateDeviceExtensions(physicalDevice);
        const bool presentIdExtensionAvailable = engine::ensureExtensionsPresent(std::vector<const char*>{VK_KHR_PRESENT_ID_EXTENSION_NAME}, availableExtensions);
        static_assert(sizeof(PFN_vkGetPhysicalDeviceFeatures2KHR) == sizeof(PFN_vkVoidFunction), "Vulkan function pointer sizes must match");
        PFN_vkGetPhysicalDeviceFeatures2KHR getFeatures2 = nullptr;
        if (const auto rawGetFeatures2KHR = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"); rawGetFeatures2KHR != nullptr) {
            getFeatures2 = std::bit_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(rawGetFeatures2KHR);
        }
        if (getFeatures2 == nullptr) {
            if (const auto rawGetFeatures2 = vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"); rawGetFeatures2 != nullptr) {
                getFeatures2 = std::bit_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(rawGetFeatures2);
            }
        }
        VkPhysicalDeviceVulkan12Features vulkan12Features = {
            .sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext                                        = nullptr,
            .storageBuffer8BitAccess                      = VK_TRUE,
            .shaderInt8                                   = VK_TRUE,
            .descriptorIndexing                           = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing    = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingPartiallyBound              = VK_TRUE,
            .descriptorBindingVariableDescriptorCount     = VK_TRUE,
            .runtimeDescriptorArray                       = VK_TRUE,
            .scalarBlockLayout                            = VK_TRUE,
            .bufferDeviceAddress                          = VK_TRUE,
        };
        VkPhysicalDeviceMaintenance4Features maintenance4Features = {
            .sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
            .pNext        = &vulkan12Features,
            .maintenance4 = VK_TRUE,
        };
        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
            .sType                                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .pNext                                  = &maintenance4Features,
            .taskShader                             = VK_TRUE,
            .meshShader                             = VK_TRUE,
            .multiviewMeshShader                    = VK_FALSE,
            .primitiveFragmentShadingRateMeshShader = VK_FALSE,
            .meshShaderQueries                      = VK_FALSE,
        };
        VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesQuery = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
            .pNext = &meshShaderFeatures,
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures = {
            .sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext                 = &presentIdFeaturesQuery,
            .accelerationStructure = VK_TRUE,
        };
        VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {
            .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext    = &accelFeatures,
            .rayQuery = VK_TRUE,
        };
        presentIdSupported_ = false;
        if (presentIdExtensionAvailable && getFeatures2 != nullptr) {
            VkPhysicalDeviceFeatures2 features2 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &presentIdFeaturesQuery,
            };
            getFeatures2(physicalDevice, &features2);
            if (presentIdFeaturesQuery.presentId == VK_TRUE) {
                presentIdSupported_ = true;
                enabledExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
            }
        }
        meshShaderFeatures.multiviewMeshShader                       = VK_FALSE;
        meshShaderFeatures.primitiveFragmentShadingRateMeshShader    = VK_FALSE;
        VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesEnable = {
            .sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
            .pNext     = &meshShaderFeatures,
            .presentId = VK_TRUE,
        };
        // Wire raytracing features into the chain.
        // The chain order: rayQueryFeatures -> accelFeatures -> (presentIdEnable|meshShader) -> ...
        void* rtNext = reinterpret_cast<void*>(&meshShaderFeatures);
        if (presentIdSupported_) {
            rtNext = reinterpret_cast<void*>(&presentIdFeaturesEnable);
        }
        accelFeatures.pNext           = rtNext;
        rayQueryFeatures.pNext        = &accelFeatures;
        void const*        pNextChain = &rayQueryFeatures;
        VkDeviceCreateInfo createInfo = {
            .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                = pNextChain,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos    = queueCreateInfos.data(),
            .pEnabledFeatures     = &deviceFeatures,
        };
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledLayerCount       = 0;
        createInfo.ppEnabledLayerNames     = nullptr;
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create logical device!");
        }
        rayQuerySupported_ = true;
        vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
        vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT) vkGetDeviceProcAddr(device_, "vkCmdDrawMeshTasksEXT");
        if (vkCmdDrawMeshTasksEXT == nullptr) {
            engine::Logger::error(engine::LogChannel::Render, "Failed to load vkCmdDrawMeshTasksEXT function pointer!");
        }
    }
    /**
 * @brief Creates a command pool for allocating Vulkan command buffers.
 * @throws std::runtime_error if command pool creation fails.
 */
    void Device::createCommandPool() {
        QueueFamilyIndices const queueFamilyIndices = findPhysicalQueueFamilies();
        VkCommandPoolCreateInfo  poolInfo           = {};
        poolInfo.sType                              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex                   = queueFamilyIndices.graphicsFamily;
        poolInfo.flags                              = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create command pool!");
        }
        VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(device_, &fenceInfo, nullptr, &singleTimeFence_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create single-time commands fence!");
        }
    }
    void Device::enableThreadLocalCommandPools() {
        if (threadLocalCommandPools_)
            return;
        threadLocalCommandPools_ = std::make_unique<ThreadLocalCommandPool>();
        threadLocalCommandPools_->init(device_, findPhysicalQueueFamilies().graphicsFamily);
    }
    VkResult Device::submitGraphics(const VkSubmitInfo* submitInfo, VkFence fence) {
        std::scoped_lock const lock(queueSubmitMutex_);
        const int              maxRetries = 2;
        VkResult               lastRes    = VK_ERROR_INITIALIZATION_FAILED;
        for (int attempt = 0; attempt <= maxRetries; ++attempt) {
            lastRes = vkQueueSubmit(graphicsQueue_, 1, submitInfo, fence);
            if (lastRes == VK_SUCCESS) {
                return lastRes;
            }
            uint32_t cbCount = (submitInfo != nullptr) ? submitInfo->commandBufferCount : 0u;
            engine::Logger::error(engine::LogChannel::Render, "[Device] submitGraphics failed: VkResult=", lastRes, " commandBuffers=", cbCount, " attempt=", attempt, " thread=", std::this_thread::get_id());
            if (lastRes == VK_ERROR_DEVICE_LOST) {
                engine::Logger::error(engine::LogChannel::Render, "[Device] VK_ERROR_DEVICE_LOST: physical device=", properties.deviceName, " vendor=", properties.vendorID, " driver=", properties.driverVersion);
                break;
            }
            if (attempt < maxRetries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return lastRes;
    }
    VkResult Device::present(const VkPresentInfoKHR* presentInfo) {
        std::scoped_lock const lock(queueSubmitMutex_);
        return vkQueuePresentKHR(presentQueue_, const_cast<VkPresentInfoKHR*>(presentInfo));
    }
    /**
 * @brief Creates the Vulkan surface for window presentation.
 */
    void Device::createSurface() {
        window.createWindowSurface(instance, &surface_);
    }
    /**
 * @brief Checks if a physical device is suitable for engine requirements.
 * @param device Vulkan physical device handle.
 * @return true if device is suitable, false otherwise.
 */
    bool Device::isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices const indices             = findQueueFamilies(device);
        bool const               extensionsSupported = checkDeviceExtensionSupport(device);
        bool                     swapChainAdequate   = false;
        if (extensionsSupported) {
            SwapChainSupportDetails const swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate                              = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        vulkan12Features.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceFeatures2 features2               = {};
        features2.sType                                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext                                   = &vulkan12Features;
        vkGetPhysicalDeviceFeatures2(device, &features2);
        bool const bindlessSupported = (vulkan12Features.descriptorIndexing != 0u) && (vulkan12Features.shaderSampledImageArrayNonUniformIndexing != 0u) &&
                                       (vulkan12Features.descriptorBindingPartiallyBound != 0u) && (vulkan12Features.descriptorBindingVariableDescriptorCount != 0u) &&
                                       (vulkan12Features.runtimeDescriptorArray != 0u) && (vulkan12Features.bufferDeviceAddress != 0u);
        return indices.isComplete() && extensionsSupported && swapChainAdequate && (supportedFeatures.samplerAnisotropy != 0u) && (supportedFeatures.shaderInt64 != 0u) && bindlessSupported;
    }
    bool Device::checkValidationLayerSupport() const {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for (const char* layerName : validationLayers) {
            const bool found = std::ranges::any_of(availableLayers, [layerName](const VkLayerProperties& layerProps) { return std::strcmp(layerName, layerProps.layerName) == 0; });
            if (!found) {
                return false;
            }
        }
        return true;
    }
    std::vector<const char*> Device::getRequiredExtensions() const {
        uint32_t                 glfwExtensionCount = 0;
        const char**             glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }
    void Device::setupDebugMessenger() {
        if (!enableValidationLayers) {
            return;
        }
        debugMessenger = std::make_unique<DebugMessenger>();
        debugMessenger->create(instance);
    }
    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices{};
        uint32_t           queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)) {
                indices.graphicsFamily         = i;
                indices.graphicsFamilyHasValue = true;
            }
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport == VK_TRUE) {
                indices.presentFamily         = i;
                indices.presentFamilyHasValue = true;
            }
            if (indices.isComplete()) {
                break;
            }
            ++i;
        }
        return indices;
    }
    void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo                 = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData       = nullptr;
    }
    bool Device::hasGlfwRequiredInstanceExtensions() {
        uint32_t                 glfwExtensionCount = 0;
        const char**             glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> required(glfwExtensions, glfwExtensions + glfwExtensionCount);
        const auto               available = engine::enumerateInstanceExtensions();
        return engine::ensureExtensionsPresent(required, available);
    }
    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
        const auto               availableExtensions = engine::enumerateDeviceExtensions(device);
        std::vector<const char*> required(deviceExtensions.begin(), deviceExtensions.end());
        return engine::ensureExtensionsPresent(required, availableExtensions);
    }
    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        }
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
        }
        return details;
    }
    VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        if (candidates.empty()) {
            throw engine::RuntimeException("findSupportedFormat: candidates list is empty!");
        }
        for (VkFormat const format : candidates) {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
            if (tiling == VK_IMAGE_TILING_LINEAR && (formatProperties.linearTilingFeatures & features) == features) {
                return format;
            }
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        std::string tilingStr = (tiling == VK_IMAGE_TILING_LINEAR) ? "LINEAR" : "OPTIMAL";
        engine::Logger::error(engine::LogChannel::Render, "[Vulkan] findSupportedFormat failed: tiling=", tilingStr, " features=0x", std::hex, features, std::dec, " tested=", candidates.size(), " formats");
        for (size_t i = 0; i < candidates.size(); ++i) {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &formatProperties);
            engine::Logger::error(engine::LogChannel::Render, "  Format ", candidates[i], ": ");
            if (tiling == VK_IMAGE_TILING_LINEAR) {
                engine::Logger::error(engine::LogChannel::Render, " linear=0x", std::hex, formatProperties.linearTilingFeatures, std::dec);
            } else {
                engine::Logger::error(engine::LogChannel::Render, " optimal=0x", std::hex, formatProperties.optimalTilingFeatures, std::dec);
            }
        }
        throw engine::RuntimeException("failed to find supported format! See error output above for details.");
    }
    VkCommandBuffer Device::beginSingleTimeCommands() {
        VkCommandPool pool = VK_NULL_HANDLE;
        if (threadLocalCommandPools_) {
            pool = threadLocalCommandPools_->getForCurrentThread();
        } else {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = findPhysicalQueueFamilies().graphicsFamily;
            VkCommandPool tempPool    = VK_NULL_HANDLE;
            if (vkCreateCommandPool(device_, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
                throw engine::RuntimeException("failed to create temporary command pool for single-time commands");
            }
            pool = tempPool;
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool        = pool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
        {
            std::scoped_lock const lock(singleCmdMutex);
            cmdBufferToPoolMap_[commandBuffer] = pool;
        }
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }
    void Device::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;
        std::scoped_lock const fenceLock(singleCmdMutex);
        vkResetFences(device_, 1, &singleTimeFence_);
        VkResult const submitRes = submitGraphics(&submitInfo, singleTimeFence_);
        if (submitRes != VK_SUCCESS) {
            throw engine::RuntimeException("failed to submit single-time command buffer: " + std::to_string(submitRes));
        }
        constexpr uint64_t timeoutNs = 10ull * 1000ull * 1000ull * 1000ull;
        VkResult const     waitRes   = vkWaitForFences(device_, 1, &singleTimeFence_, VK_TRUE, timeoutNs);
        if (waitRes != VK_SUCCESS) {
            throw engine::RuntimeException("vkWaitForFences failed: " + std::to_string(waitRes));
        }
        VkCommandPool pool = VK_NULL_HANDLE;
        {
            auto it = cmdBufferToPoolMap_.find(commandBuffer);
            if (it != cmdBufferToPoolMap_.end()) {
                pool = it->second;
                cmdBufferToPoolMap_.erase(it);
            }
        }
        if (pool != VK_NULL_HANDLE) {
            if (threadLocalCommandPools_ && threadLocalCommandPools_->ownsPool(pool)) {
                vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
            } else {
                vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
                vkDestroyCommandPool(device_, pool, nullptr);
            }
        } else {
            vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
        }
    }
    VkResult Device::allocateSecondaryCommandBuffer(VkCommandBuffer* outCommandBuffer) {
        if (!outCommandBuffer)
            return VK_ERROR_INITIALIZATION_FAILED;
        VkCommandPool pool = commandPool;
        if (threadLocalCommandPools_) {
            pool = threadLocalCommandPools_->getForCurrentThread();
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = 1;
        VkResult res                 = vkAllocateCommandBuffers(device_, &allocInfo, outCommandBuffer);
        if (res == VK_SUCCESS) {
            std::scoped_lock const lock(singleCmdMutex);
            cmdBufferToPoolMap_.emplace(*outCommandBuffer, pool);
        }
        return res;
    }
    void Device::freeSecondaryCommandBuffer(VkCommandBuffer commandBuffer) {
        VkCommandPool pool = VK_NULL_HANDLE;
        {
            std::scoped_lock const lock(singleCmdMutex);
            auto                   it = cmdBufferToPoolMap_.find(commandBuffer);
            if (it != cmdBufferToPoolMap_.end()) {
                pool = it->second;
                cmdBufferToPoolMap_.erase(it);
            }
        }
        if (pool == VK_NULL_HANDLE)
            pool = commandPool;
        vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
    }
}  // namespace engine