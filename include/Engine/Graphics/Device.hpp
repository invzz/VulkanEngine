#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICE_HPP
#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
namespace engine {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };
    struct QueueFamilyIndices {
        uint32_t           graphicsFamily;
        uint32_t           presentFamily;
        bool               graphicsFamilyHasValue = false;
        bool               presentFamilyHasValue  = false;
        [[nodiscard]] bool isComplete() const {
            return graphicsFamilyHasValue && presentFamilyHasValue;
        }
    };
    class Device {
       public:
        static constexpr uint32_t kMaxFramesInFlight = 2;
        struct SamplerCacheStats {
            uint64_t cacheHits{0};
            uint64_t cacheMisses{0};
            uint64_t cachedSamplers{0};
        };
#ifndef ENGINE_ENABLE_VALIDATION
#define ENGINE_ENABLE_VALIDATION 1
#endif
        static constexpr bool kBuildValidationLayersEnabled = (ENGINE_ENABLE_VALIDATION != 0);
        static void           setValidationLayersEnabledOverride(bool enabled);
        static void           clearValidationLayersEnabledOverride();
        void                  WaitIdle() {
            vkDeviceWaitIdle(device_);
        }
        explicit Device(Window& window);
        ~Device();
        Device(const Device&)             = delete;
        Device& operator=(const Device&)  = delete;
        Device(Device&&)                  = delete;
        Device&       operator=(Device&&) = delete;
        VkCommandPool getCommandPool() {
            return commandPool;
        }
        DeviceMemory& getMemory() {
            return *memory_;
        }
        VkDevice device() {
            return device_;
        }
        VkSurfaceKHR surface() {
            return surface_;
        }
        VkQueue graphicsQueue() {
            return graphicsQueue_;
        }
        VkQueue presentQueue() {
            return presentQueue_;
        }
        VkInstance getInstance() {
            return instance;
        }
        [[nodiscard]] bool supportsPresentId() const {
            return presentIdSupported_;
        }
        SwapChainSupportDetails getSwapChainSupport() {
            return querySwapChainSupport(physicalDevice);
        }
        QueueFamilyIndices findPhysicalQueueFamilies() {
            return findQueueFamilies(physicalDevice);
        }
        VkFormat      findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        DeviceMemory& memory() {
            return *memory_;
        }
        [[nodiscard]] const VkPhysicalDeviceProperties& getProperties() const {
            return properties;
        }
        [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const {
            return physicalDevice;
        }
        VkCommandBuffer beginSingleTimeCommands();
        void            endSingleTimeCommands(VkCommandBuffer commandBuffer);
        VkResult        allocateSecondaryCommandBuffer(VkCommandBuffer* outCommandBuffer);
        void            freeSecondaryCommandBuffer(VkCommandBuffer commandBuffer);
        void            enableThreadLocalCommandPools();
        bool            threadLocalCommandPoolsEnabled() const {
            return threadLocalCommandPools_ != nullptr;
        }
        VkResult submitGraphics(const VkSubmitInfo* submitInfo, VkFence fence);
        VkResult present(const VkPresentInfoKHR* presentInfo);
        void     setCurrentFrameIndex(uint32_t frameIndex);
        uint32_t getCurrentFrameIndex() const {
            return currentFrameIndex_;
        }
        void                            deferDestroy(std::function<void(VkDevice)> fn);
        void                            flushDeferred(uint32_t frameIndex);
        void                            flushAllDeferred();
        VkSampler                       getOrCreateSampler(const VkSamplerCreateInfo& createInfo);
        [[nodiscard]] SamplerCacheStats getSamplerCacheStats() const;
        PFN_vkCmdDrawMeshTasksEXT       vkCmdDrawMeshTasksEXT = nullptr;

       private:
        [[nodiscard]] bool                                                         checkValidationLayerSupport() const;
        [[nodiscard]] std::vector<const char*>                                     getRequiredExtensions() const;
        void                                                                       createInstance();
        void                                                                       setupDebugMessenger();
        void                                                                       createSurface();
        void                                                                       pickPhysicalDevice();
        void                                                                       createLogicalDevice();
        void                                                                       createCommandPool();
        bool                                                                       isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices                                                         findQueueFamilies(VkPhysicalDevice device);
        static void                                                                populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        static bool                                                                hasGlfwRequiredInstanceExtensions();
        bool                                                                       checkDeviceExtensionSupport(VkPhysicalDevice device) const;
        SwapChainSupportDetails                                                    querySwapChainSupport(VkPhysicalDevice device);
        [[nodiscard]] static bool                                                  resolveValidationLayersEnabled();
        VkInstance                                                                 instance = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties                                                 properties;
        std::unique_ptr<class DebugMessenger>                                      debugMessenger;
        std::unique_ptr<class ThreadLocalCommandPool>                              threadLocalCommandPools_;
        VkPhysicalDevice                                                           physicalDevice         = VK_NULL_HANDLE;
        bool                                                                       enableValidationLayers = kBuildValidationLayersEnabled;
        Window&                                                                    window;
        VkCommandPool                                                              commandPool      = VK_NULL_HANDLE;
        VkDevice                                                                   device_          = VK_NULL_HANDLE;
        VkSurfaceKHR                                                               surface_         = VK_NULL_HANDLE;
        VkQueue                                                                    graphicsQueue_   = VK_NULL_HANDLE;
        VkQueue                                                                    presentQueue_    = VK_NULL_HANDLE;
        const std::vector<const char*>                                             validationLayers = {"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        };
        static std::atomic<int>                                                    validationLayersOverride_;
        bool                                                                       presentIdSupported_ = false;
        std::unique_ptr<DeviceMemory>                                              memory_;
        uint32_t                                                                   currentFrameIndex_ = 0;
        std::array<std::vector<std::function<void(VkDevice)>>, kMaxFramesInFlight> deferredDestroy_;
        mutable std::mutex                                                         deferredDestroyMutex_;
        mutable std::mutex                                                         singleCmdMutex;
        mutable std::unordered_map<VkCommandBuffer, VkCommandPool>                 cmdBufferToPoolMap_;
        VkFence                                                                    singleTimeFence_ = VK_NULL_HANDLE;
        struct SamplerCacheKey {
            uint32_t magFilter;
            uint32_t minFilter;
            uint32_t mipmapMode;
            uint32_t addressModeU;
            uint32_t addressModeV;
            uint32_t addressModeW;
            uint32_t anisotropyEnable;
            uint32_t maxAnisotropyBits;
            uint32_t compareEnable;
            uint32_t compareOp;
            uint32_t minLodBits;
            uint32_t maxLodBits;
            uint32_t mipLodBiasBits;
            uint32_t borderColor;
            uint32_t unnormalizedCoordinates;
            bool     operator==(const SamplerCacheKey& other) const {
                return magFilter == other.magFilter && minFilter == other.minFilter && mipmapMode == other.mipmapMode && addressModeU == other.addressModeU &&
                       addressModeV == other.addressModeV && addressModeW == other.addressModeW && anisotropyEnable == other.anisotropyEnable &&
                       maxAnisotropyBits == other.maxAnisotropyBits && compareEnable == other.compareEnable && compareOp == other.compareOp &&
                       minLodBits == other.minLodBits && maxLodBits == other.maxLodBits && mipLodBiasBits == other.mipLodBiasBits &&
                       borderColor == other.borderColor && unnormalizedCoordinates == other.unnormalizedCoordinates;
            }
        };
        struct SamplerCacheKeyHash {
            size_t operator()(const SamplerCacheKey& key) const;
        };
        [[nodiscard]] SamplerCacheKey                                       makeSamplerCacheKey(const VkSamplerCreateInfo& createInfo) const;
        void                                                                destroySamplerCache();
        mutable std::mutex                                                  samplerCacheMutex_;
        std::unordered_map<SamplerCacheKey, VkSampler, SamplerCacheKeyHash> samplerCache_;
        uint64_t                                                            samplerCacheHits_   = 0;
        uint64_t                                                            samplerCacheMisses_ = 0;
        mutable std::mutex                                                  queueSubmitMutex_;
        friend class DeviceMemory;
    };
}  // namespace engine
#endif
