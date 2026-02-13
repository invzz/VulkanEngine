#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_EXTENSIONHELPERS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_EXTENSIONHELPERS_HPP

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace engine {

// Enumerate available instance extensions. Returns an empty vector on error.
std::vector<VkExtensionProperties> enumerateInstanceExtensions();

// Enumerate available device extensions for a physical device. Returns an empty vector on error.
std::vector<VkExtensionProperties> enumerateDeviceExtensions(VkPhysicalDevice device);

// Verify that every name in `required` appears in the `available` extension list.
bool ensureExtensionsPresent(const std::vector<const char*>& required, const std::vector<VkExtensionProperties>& available);

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_EXTENSIONHELPERS_HPP
