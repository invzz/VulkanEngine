#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_THREADLOCALCOMMANDPOOL_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_THREADLOCALCOMMANDPOOL_HPP

#include <vulkan/vulkan_core.h>

#include <mutex>
#include <thread>
#include <unordered_map>

namespace engine {

  class ThreadLocalCommandPool
  {
  public:
    ThreadLocalCommandPool() = default;
    ThreadLocalCommandPool(VkDevice device, uint32_t queueFamilyIndex) { init(device, queueFamilyIndex); }

    // Initialize the manager with a device and queue-family index.
    void init(VkDevice device, uint32_t queueFamilyIndex);

    // Returns a VkCommandPool for the calling thread, creating one lazily.
    VkCommandPool getForCurrentThread();

    // Destroy the pool for the calling thread. No-op if none is present.
    void destroyForCurrentThread() noexcept;

    // Destroy all pools created by this manager. Safe to call from any thread.
    void destroyAll() noexcept;

  private:
    VkDevice device_           = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;

    std::mutex                                         mutex_;
    std::unordered_map<std::thread::id, VkCommandPool> pools_;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_THREADLOCALCOMMANDPOOL_HPP
