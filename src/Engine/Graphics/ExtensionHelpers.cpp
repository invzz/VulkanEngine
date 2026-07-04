#include "Engine/Graphics/ExtensionHelpers.hpp"

#include <vulkan/vulkan_core.h>

#include <cstring>

#include "Engine/Core/Logger.hpp"
namespace engine {
    std::vector<VkExtensionProperties> enumerateInstanceExtensions() {
        uint32_t extensionCount = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
            engine::Logger::error(engine::LogChannel::Render, "[ExtensionHelpers] vkEnumerateInstanceExtensionProperties failed");
            return {};
        }
        std::vector<VkExtensionProperties> available(extensionCount);
        if (extensionCount != 0) {
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, available.data());
        }
        return available;
    }
    std::vector<VkExtensionProperties> enumerateDeviceExtensions(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
            engine::Logger::error(engine::LogChannel::Render, "[ExtensionHelpers] vkEnumerateDeviceExtensionProperties failed");
            return {};
        }
        std::vector<VkExtensionProperties> available(extensionCount);
        if (extensionCount != 0) {
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());
        }
        return available;
    }
    bool ensureExtensionsPresent(const std::vector<const char*>& required, const std::vector<VkExtensionProperties>& available) {
        for (const char* name : required) {
            bool found = false;
            for (const auto& ext : available) {
                if (std::strcmp(ext.extensionName, name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
}  // namespace engine
