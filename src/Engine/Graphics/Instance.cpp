#include "Engine/Graphics/Instance.hpp"

#include "Engine/Core/Exceptions.hpp"

namespace engine {

    Instance::~Instance() {
        reset();
    }

    void Instance::create(const std::vector<const char*>& extensions, const std::vector<const char*>& layers) {
        if (instance_ != VK_NULL_HANDLE)
            return;

        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "EngineInstance";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "VulkanEngine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
        createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create instance");
        }
    }

    void Instance::reset() noexcept {
        if (instance_ == VK_NULL_HANDLE)
            return;

        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

}  // namespace engine
