#include "Engine/Graphics/Device.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/DebugMessenger.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/ExtensionHelpers.hpp"
#include "Engine/Graphics/ThreadLocalCommandPool.hpp"
#include "GLFW/glfw3.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"

// std headers
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

// Use Vulkan SDK loader (vulkan-1) - include normal Vulkan headers

namespace {
// File-local Vulkan debug helpers.
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {
  if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u) {
    // General message
    std::cerr << "[ " << GREEN "GENERAL" RESET;
  } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u) {
    // Validation message
    std::cerr << "[ " << YELLOW "VALIDATION" RESET;
  } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
    // Performance message
    std::cerr << "[ " << BLUE "PERFORMANCE" RESET;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
    std::cerr << RED " ERROR" RESET " ] " << pCallbackData->pMessage << "\n";
    return VK_FALSE;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
    std::cerr << YELLOW " WARNING" RESET " ] " << pCallbackData->pMessage << "\n";
    return VK_FALSE;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0) {
    std::cerr << BLUE " INFO" RESET " ] " << pCallbackData->pMessage << "\n";
    return VK_FALSE;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0) {
    std::cerr << CYAN " VERBOSE" RESET " ] " << pCallbackData->pMessage << "\n";
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

void Device::setCurrentFrameIndex(uint32_t frameIndex) {
  currentFrameIndex_ = frameIndex % kMaxFramesInFlight;
}

void Device::deferDestroy(std::function<void(VkDevice)> fn) {
  deferredDestroy_[currentFrameIndex_].push_back(std::move(fn));
}

void Device::flushDeferred(uint32_t frameIndex) {
  auto& bucket = deferredDestroy_[frameIndex % kMaxFramesInFlight];
  for (auto& fn : bucket) {
    fn(device_);
  }
  bucket.clear();
}

void Device::flushAllDeferred() {
  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    flushDeferred(i);
  }
}

// class member functions
/**
 * @class Device
 * @brief Manages Vulkan device, queues, and related resources.
 * @param window Reference to the main application window.
 */
Device::Device(Window& window) : window{window} {
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();
  // initialize memory helper (depends on device_ and commandPool being
  // created)
  memory_ = std::make_unique<DeviceMemory>(*this);
}

/**
 * @brief Destructor. Cleans up Vulkan resources and device.
 */
Device::~Device() {
  // Make destructor safe in partial-construction and avoid throwing.
  try {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);
    }

    // Run any deferred destroys queued by subsystems. Protect against
    // user-provided callbacks throwing.
    try {
      if (device_ != VK_NULL_HANDLE) {
        flushAllDeferred();
      }
    } catch (const std::exception& e) {
      std::cerr << "[Device::~Device] flushAllDeferred threw: " << e.what() << "\n";
    } catch (...) {
      std::cerr << "[Device::~Device] flushAllDeferred threw unknown exception\n";
    }

    // ensure helper is destroyed before device/command pool teardown
    try {
      memory_.reset();
    } catch (const std::exception& e) {
      std::cerr << "[Device::~Device] DeviceMemory destructor threw: " << e.what() << "\n";
    } catch (...) {
      std::cerr << "[Device::~Device] DeviceMemory destructor threw unknown exception\n";
    }

    // Destroy any thread-local command pools before destroying the device
    // to ensure pool destruction happens while the device is still valid.
    try {
      if (threadLocalCommandPools_) {
        threadLocalCommandPools_->destroyAll();
        threadLocalCommandPools_.reset();
      }
    } catch (const std::exception& e) {
      std::cerr << "[Device::~Device] destroyAll threadLocalCommandPools threw: " << e.what() << "\n";
    } catch (...) {
      std::cerr << "[Device::~Device] destroyAll threadLocalCommandPools threw unknown exception\n";
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
        // Destroy via RAII guard.
        try {
          debugMessenger.reset();
        } catch (const std::exception& e) {
          std::cerr << "[Device::~Device] DebugMessenger destructor threw: " << e.what() << "\n";
        } catch (...) {
          std::cerr << "[Device::~Device] DebugMessenger destructor threw unknown exception\n";
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
    std::cerr << "[Device::~Device] Exception during shutdown: " << e.what() << "\n";
  } catch (...) {
    std::cerr << "[Device::~Device] Unknown exception during shutdown\n";
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
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "LittleVulkanEngine App",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };

  VkInstanceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
  };

  auto extensions = getRequiredExtensions();
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // When validation layers are enabled we set the layer list on the
  // instance create info. We do NOT attach the DebugUtils create info to
  // `pNext` here because we create the debug messenger explicitly in
  // `setupDebugMessenger()` — this gives us an explicit handle that we can
  // destroy during shutdown.
  if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.pNext = nullptr;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create instance!");
  }

  // Using Vulkan SDK loader (vulkan-1) - no explicit loader init required

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

  VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties best = {};

  for (const auto& device : devices) {
    if (!isDeviceSuitable(device)) continue;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    if (bestDevice == VK_NULL_HANDLE || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && best.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        props.deviceType == best.deviceType && props.limits.maxImageDimension2D > best.limits.maxImageDimension2D) {
      bestDevice = device;
      best = props;
    }
  }

  if (bestDevice == VK_NULL_HANDLE) {
    throw engine::RuntimeException("failed to find a suitable GPU!");
  }

  physicalDevice = bestDevice;
  properties = best;
  std::cout << "physical device: " << properties.deviceName << "\n";
}

void Device::createLogicalDevice() {
  QueueFamilyIndices const indices = findQueueFamilies(physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> const uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

  float const queuePriority = 1.0f;

  for (uint32_t const queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo const queueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &queuePriority};

    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures const deviceFeatures = {
      .samplerAnisotropy = VK_TRUE,
      .shaderInt64 = VK_TRUE,
  };

  std::vector<const char*> enabledExtensions(deviceExtensions.begin(), deviceExtensions.end());
  enabledExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);

  const auto availableExtensions = engine::enumerateDeviceExtensions(physicalDevice);

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

  // Enable Vulkan 1.2 features for Bindless Rendering
  VkPhysicalDeviceVulkan12Features vulkan12Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = nullptr,
      .storageBuffer8BitAccess = VK_TRUE,  // Required for shaders using uint8_t storage buffers
      .shaderInt8 = VK_TRUE,               // Required for shaders using int8/uint8 types
      .descriptorIndexing = VK_TRUE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,  // Required for TextureManager UPDATE_AFTER_BIND
      .descriptorBindingPartiallyBound = VK_TRUE,
      .descriptorBindingVariableDescriptorCount = VK_TRUE,
      .runtimeDescriptorArray = VK_TRUE,
      .scalarBlockLayout = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };

  VkPhysicalDeviceMaintenance4Features maintenance4Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
      .pNext = &vulkan12Features,
      .maintenance4 = VK_TRUE,
  };

  VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
      .pNext = &maintenance4Features,
      .taskShader = VK_TRUE,
      .meshShader = VK_TRUE,
      .multiviewMeshShader = VK_FALSE,
      .primitiveFragmentShadingRateMeshShader = VK_FALSE,
      .meshShaderQueries = VK_FALSE,
  };

  VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesQuery = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
      .pNext = &meshShaderFeatures,
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

  // Reset unsupported/unwanted mesh shader features that might have been
  // enabled by the query
  meshShaderFeatures.multiviewMeshShader = VK_FALSE;
  meshShaderFeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE;

  VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesEnable = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
      .pNext = &meshShaderFeatures,  // Chain to meshShaderFeatures
      .presentId = VK_TRUE,
  };

  // Set up pNext chain: presentId (if supported) -> meshShaderFeatures ->
  // vulkan12Features
  void const* pNextChain = &meshShaderFeatures;
  if (presentIdSupported_) {
    pNextChain = &presentIdFeaturesEnable;
  }

  VkDeviceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = pNextChain,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();

  // might not really be necessary anymore because device specific
  // validation layers have been deprecated
  if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create logical device!");
  }

  vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);

  // Using Vulkan SDK loader (vulkan-1) - device-level function pointers are
  // resolved via vkGetDeviceProcAddr

  vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(device_, "vkCmdDrawMeshTasksEXT");
  if (vkCmdDrawMeshTasksEXT == nullptr) {
    std::cerr << "Failed to load vkCmdDrawMeshTasksEXT function pointer!" << "\n";
  }
}

/**
 * @brief Creates a command pool for allocating Vulkan command buffers.
 * @throws std::runtime_error if command pool creation fails.
 */
void Device::createCommandPool() {
  QueueFamilyIndices const queueFamilyIndices = findPhysicalQueueFamilies();

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create command pool!");
  }

  // Create reusable fence for single-time commands
  VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  if (vkCreateFence(device_, &fenceInfo, nullptr, &singleTimeFence_) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create single-time commands fence!");
  }
}

void Device::enableThreadLocalCommandPools() {
  if (threadLocalCommandPools_) return;  // already enabled
  threadLocalCommandPools_ = std::make_unique<ThreadLocalCommandPool>();
  threadLocalCommandPools_->init(device_, findPhysicalQueueFamilies().graphicsFamily);
}

VkResult Device::submitGraphics(const VkSubmitInfo* submitInfo, VkFence fence) {
  std::scoped_lock const lock(queueSubmitMutex_);
  const int maxRetries = 2;
  VkResult lastRes = VK_ERROR_INITIALIZATION_FAILED;
  for (int attempt = 0; attempt <= maxRetries; ++attempt) {
    lastRes = vkQueueSubmit(graphicsQueue_, 1, submitInfo, fence);
    if (lastRes == VK_SUCCESS) {
      return lastRes;
    }

    uint32_t cbCount = (submitInfo != nullptr) ? submitInfo->commandBufferCount : 0u;
    std::cerr << "[Device] submitGraphics failed: VkResult=" << lastRes << " commandBuffers=" << cbCount << " attempt=" << attempt << " thread=" << std::this_thread::get_id() << "\n";

    if (lastRes == VK_ERROR_DEVICE_LOST) {
      // Dump physical device info to help debugging device lost errors
      std::cerr << "[Device] VK_ERROR_DEVICE_LOST: physical device: " << properties.deviceName << " vendor=" << properties.vendorID << " driver=" << properties.driverVersion << "\n";
      break;  // no point retrying
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
  QueueFamilyIndices const indices = findQueueFamilies(device);

  bool const extensionsSupported = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails const swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  VkPhysicalDeviceVulkan12Features vulkan12Features = {};
  vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

  VkPhysicalDeviceFeatures2 features2 = {};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &vulkan12Features;

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
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

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

  // Use the RAII pilot DebugMessenger to manage creation and destruction.
  debugMessenger = std::make_unique<DebugMessenger>();
  debugMessenger->create(instance);
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices{};

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  uint32_t i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueCount > 0 && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)) {
      indices.graphicsFamily = i;
      indices.graphicsFamilyHasValue = true;
    }

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
    if (queueFamily.queueCount > 0 && presentSupport == VK_TRUE) {
      indices.presentFamily = i;
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
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  createInfo.pUserData = nullptr;
}

bool Device::hasGlfwRequiredInstanceExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char*> required(glfwExtensions, glfwExtensions + glfwExtensionCount);

  const auto available = engine::enumerateInstanceExtensions();

  return engine::ensureExtensionsPresent(required, available);
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
  const auto availableExtensions = engine::enumerateDeviceExtensions(device);
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

  // If we get here, no suitable format was found. Log details for debugging.
  std::string tilingStr = (tiling == VK_IMAGE_TILING_LINEAR) ? "LINEAR" : "OPTIMAL";
  std::cerr << "[ " << RED "ERROR" RESET " ] findSupportedFormat failed:\n"
            << "  Tiling: " << tilingStr << "\n"
            << "  Required features: 0x" << std::hex << features << std::dec << "\n"
            << "  Tested " << candidates.size() << " candidate formats:\n";

  for (size_t i = 0; i < candidates.size(); ++i) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &formatProperties);
    std::cerr << "    [" << i << "] Format " << candidates[i] << ": ";
    if (tiling == VK_IMAGE_TILING_LINEAR) {
      std::cerr << "linear=0x" << std::hex << formatProperties.linearTilingFeatures << std::dec;
    } else {
      std::cerr << "optimal=0x" << std::hex << formatProperties.optimalTilingFeatures << std::dec;
    }
    std::cerr << "\n";
  }

  throw engine::RuntimeException("failed to find supported format! See error output above for details.");
}

VkCommandBuffer Device::beginSingleTimeCommands() {
  VkCommandPool pool = VK_NULL_HANDLE;
  // If an opt-in thread-local manager is enabled, use it. Otherwise create a
  // temporary pool per call (current behavior).
  if (threadLocalCommandPools_) {
    pool = threadLocalCommandPools_->getForCurrentThread();
  } else {
    // Create a temporary command pool for this single-time command buffer so worker
    // threads can allocate/record independently without contending on a shared
    // command pool (avoids threading validation errors).
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = findPhysicalQueueFamilies().graphicsFamily;

    VkCommandPool tempPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
      throw engine::RuntimeException("failed to create temporary command pool for single-time commands");
    }
    pool = tempPool;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

  // Remember which pool owns this command buffer so endSingleTimeCommands can
  // free (and possibly destroy) the pool when done.
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
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  // Use the reusable fence (protected by singleCmdMutex for thread safety)
  std::scoped_lock const fenceLock(singleCmdMutex);
  vkResetFences(device_, 1, &singleTimeFence_);

  VkResult const submitRes = submitGraphics(&submitInfo, singleTimeFence_);
  if (submitRes != VK_SUCCESS) {
    throw engine::RuntimeException("failed to submit single-time command buffer: " + std::to_string(submitRes));
  }

  constexpr uint64_t timeoutNs = 10ull * 1000ull * 1000ull * 1000ull;  // 10 seconds
  VkResult const waitRes = vkWaitForFences(device_, 1, &singleTimeFence_, VK_TRUE, timeoutNs);
  if (waitRes != VK_SUCCESS) {
    throw engine::RuntimeException("vkWaitForFences failed: " + std::to_string(waitRes));
  }

  // std::cerr << "[Device] endSingleTimeCommands - submit OK for cmdBuffer=" << commandBuffer << "\n";

  VkCommandPool pool = VK_NULL_HANDLE;
  {
    // Already holding fenceLock on singleCmdMutex - access map directly
    auto it = cmdBufferToPoolMap_.find(commandBuffer);
    if (it != cmdBufferToPoolMap_.end()) {
      pool = it->second;
      cmdBufferToPoolMap_.erase(it);
    }
  }

  if (pool != VK_NULL_HANDLE) {
    // If the pool is managed by the ThreadLocalCommandPool, only free the
    // command buffer — do not destroy the pool.
    if (threadLocalCommandPools_ && threadLocalCommandPools_->ownsPool(pool)) {
      vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
    } else {
      vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
      vkDestroyCommandPool(device_, pool, nullptr);
    }
  } else {
    // Fallback to freeing from the shared command pool if we couldn't find
    // an associated temporary pool (shouldn't happen normally).
    vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
  }
}

// Allocate a secondary command buffer. Prefer thread-local pools when enabled
// to avoid contention; remember the owning pool so the CB can be freed later.
VkResult Device::allocateSecondaryCommandBuffer(VkCommandBuffer* outCommandBuffer) {
  if (!outCommandBuffer) return VK_ERROR_INITIALIZATION_FAILED;

  VkCommandPool pool = commandPool;
  if (threadLocalCommandPools_) {
    pool = threadLocalCommandPools_->getForCurrentThread();
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = pool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  allocInfo.commandBufferCount = 1;

  VkResult res = vkAllocateCommandBuffers(device_, &allocInfo, outCommandBuffer);
  if (res == VK_SUCCESS) {
    std::scoped_lock const lock(singleCmdMutex);
    cmdBufferToPoolMap_.emplace(*outCommandBuffer, pool);
  }
  return res;
}

// Free a secondary command buffer previously allocated with allocateSecondaryCommandBuffer.
void Device::freeSecondaryCommandBuffer(VkCommandBuffer commandBuffer) {
  VkCommandPool pool = VK_NULL_HANDLE;
  {
    std::scoped_lock const lock(singleCmdMutex);
    auto it = cmdBufferToPoolMap_.find(commandBuffer);
    if (it != cmdBufferToPoolMap_.end()) {
      pool = it->second;
      cmdBufferToPoolMap_.erase(it);
    }
  }
  if (pool == VK_NULL_HANDLE) pool = commandPool;
  vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
}

}  // namespace engine