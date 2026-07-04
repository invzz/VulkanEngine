#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_INSTANCE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_INSTANCE_HPP
#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>
namespace engine {
    class Instance {
       public:
        Instance()                           = default;
        Instance(const Instance&)            = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&)                 = delete;
        Instance& operator=(Instance&&)      = delete;
        ~Instance();
        void       create(const std::vector<const char*>& extensions = {}, const std::vector<const char*>& layers = {});
        void       reset() noexcept;
        VkInstance get() const {
            return instance_;
        }
        explicit operator bool() const {
            return instance_ != VK_NULL_HANDLE;
        }

       private:
        VkInstance instance_ = VK_NULL_HANDLE;
    };
}  // namespace engine
#endif
