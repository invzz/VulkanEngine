#include "Engine/Graphics/DebugMessenger.hpp"

#include <iostream>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/ansi_colors.hpp"

namespace {
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                                                 messageType,
        const VkDebugUtilsMessengerCallbackDataEXT*                                     pCallbackData,
        void* /*pUserData*/) {
        if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u) {
            std::cerr << "[ " << "GENERAL" << " ] " << pCallbackData->pMessage << "\n";
        } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u) {
            std::cerr << "[ " << "VALIDATION" << " ] " << pCallbackData->pMessage << "\n";
        } else if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
            std::cerr << "[ " << "PERFORMANCE" << " ] " << pCallbackData->pMessage << "\n";
        }
        return VK_FALSE;
    }
}  // namespace

namespace engine {

    DebugMessenger::~DebugMessenger() {
        reset();
    }

    void DebugMessenger::create(VkInstance instance) {
        if (messenger_ != VK_NULL_HANDLE) {
            return;  // already created
        }

        instance_ = instance;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData       = nullptr;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
        if (func == nullptr) {
            throw engine::RuntimeException("vkCreateDebugUtilsMessengerEXT not available");
        }

        if (func(instance_, &createInfo, nullptr, &messenger_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create debug messenger");
        }
    }

    void DebugMessenger::reset() noexcept {
        if (messenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) {
            messenger_ = VK_NULL_HANDLE;
            return;
        }

        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance_, messenger_, nullptr);
        }
        messenger_ = VK_NULL_HANDLE;
    }

}  // namespace engine
