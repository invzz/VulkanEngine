#include "Engine/Graphics/ThreadLocalCommandPool.hpp"

#include <cassert>

#include "Engine/Core/Exceptions.hpp"

namespace engine {

  void ThreadLocalCommandPool::init(VkDevice device, uint32_t queueFamilyIndex)
  {
    if (device_ != VK_NULL_HANDLE) return; // already initialized
    device_           = device;
    queueFamilyIndex_ = queueFamilyIndex;
  }

  VkCommandPool ThreadLocalCommandPool::getForCurrentThread()
  {
    assert(device_ != VK_NULL_HANDLE && "ThreadLocalCommandPool not initialized");
    const auto tid = std::this_thread::get_id();
    {
      std::lock_guard<std::mutex> lk(mutex_);
      auto                        it = pools_.find(tid);
      if (it != pools_.end()) return it->second;
    }

    // Create a new pool for this thread
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex_;

    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create command pool for thread");
    }

    {
      std::lock_guard<std::mutex> lk(mutex_);
      // store and return
      pools_[tid] = pool;
    }

    return pool;
  }

  void ThreadLocalCommandPool::destroyForCurrentThread() noexcept
  {
    const auto                  tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lk(mutex_);
    auto                        it = pools_.find(tid);
    if (it == pools_.end()) return;
    if (device_ != VK_NULL_HANDLE && it->second != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool(device_, it->second, nullptr);
    }
    pools_.erase(it);
  }

  void ThreadLocalCommandPool::destroyAll() noexcept
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (device_ == VK_NULL_HANDLE) return;
    for (auto& kv : pools_)
    {
      if (kv.second != VK_NULL_HANDLE) vkDestroyCommandPool(device_, kv.second, nullptr);
    }
    pools_.clear();
  }

} // namespace engine
