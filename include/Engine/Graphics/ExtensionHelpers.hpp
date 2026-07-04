#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_EXTENSIONHELPERS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_EXTENSIONHELPERS_HPP
#include <vulkan/vulkan.h>

#include <vector>
namespace engine {
    std::vector<VkExtensionProperties> enumerateInstanceExtensions();
    std::vector<VkExtensionProperties> enumerateDeviceExtensions(VkPhysicalDevice device);
    bool                               ensureExtensionsPresent(const std::vector<const char*>& required, const std::vector<VkExtensionProperties>& available);
}  // namespace engine
#endif
