#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_THREADLOCALCOMMANDPOOL_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_THREADLOCALCOMMANDPOOL_HPP
#include <vulkan/vulkan_core.h>

#include <mutex>
#include <thread>
#include <unordered_map>
namespace engine {
    class ThreadLocalCommandPool {
       public:
        ThreadLocalCommandPool() = default;
        ThreadLocalCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
            init(device, queueFamilyIndex);
        }
        void          init(VkDevice device, uint32_t queueFamilyIndex);
        VkCommandPool getForCurrentThread();
        void          destroyForCurrentThread() noexcept;
        void          destroyAll() noexcept;
        bool          ownsPool(VkCommandPool pool) const;

       private:
        VkDevice                                           device_           = VK_NULL_HANDLE;
        uint32_t                                           queueFamilyIndex_ = 0;
        mutable std::mutex                                 mutex_;
        std::unordered_map<std::thread::id, VkCommandPool> pools_;
    };
}  // namespace engine
#endif
