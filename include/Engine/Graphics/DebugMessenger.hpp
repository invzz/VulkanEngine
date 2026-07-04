#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEBUG_MESSENGER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEBUG_MESSENGER_HPP
#include <vulkan/vulkan_core.h>
namespace engine {
    class DebugMessenger {
       public:
        DebugMessenger()                                 = default;
        DebugMessenger(const DebugMessenger&)            = delete;
        DebugMessenger& operator=(const DebugMessenger&) = delete;
        DebugMessenger(DebugMessenger&&)                 = delete;
        DebugMessenger& operator=(DebugMessenger&&)      = delete;
        ~DebugMessenger();
        void                     create(VkInstance instance);
        void                     reset() noexcept;
        VkDebugUtilsMessengerEXT get() const {
            return messenger_;
        }
        explicit operator bool() const {
            return messenger_ != VK_NULL_HANDLE;
        }

       private:
        VkInstance               instance_  = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
    };
}  // namespace engine
#endif
