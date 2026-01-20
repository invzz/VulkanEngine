#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DESCRIPTORS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DESCRIPTORS_HPP

#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  class DescriptorSetLayout
  {
  public:
    class Builder
    {
    public:
      explicit Builder(Device& device) : device{device} {}

      Builder& addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1, VkDescriptorBindingFlags bindingFlags = 0);
      [[nodiscard]] std::unique_ptr<DescriptorSetLayout> build() const;

    private:
      Device&                                                    device;
      std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
      std::unordered_map<uint32_t, VkDescriptorBindingFlags>     bindingFlags;
    };

    DescriptorSetLayout(Device& device, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings, const std::unordered_map<uint32_t, VkDescriptorBindingFlags>& bindingFlags);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout&)            = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

  private:
    Device&                                                    device;
    VkDescriptorSetLayout                                      descriptorSetLayout;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    friend class DescriptorWriter;
  };

  class DescriptorPool
  {
  public:
    class Builder
    {
    public:
      Builder(Device& device) : device{device} {}

      Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);
      Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
      Builder& setMaxSets(uint32_t count);
      // Opt-in: allow the pool to create overflow pools when allocation
      // requests cannot be satisfied due to fragmentation or size limits.
      Builder&                                      setAllowOverflow(bool allow);
      [[nodiscard]] std::unique_ptr<DescriptorPool> build() const;

    private:
      Device&                           device;
      std::vector<VkDescriptorPoolSize> poolSizes;
      uint32_t                          maxSets       = 1000;
      VkDescriptorPoolCreateFlags       poolFlags     = 0;
      bool                              allowOverflow = false;
    };

    DescriptorPool(Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes, bool allowOverflow = false);
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool&)            = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    // requestedPoolSizes: optional hint used when creating an overflow pool
    // after a primary allocation failure. If nullptr, the pool's creation
    // sizes are used for any fallback.
    bool allocateDescriptor(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor, const std::vector<VkDescriptorPoolSize>* requestedPoolSizes = nullptr);
    void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;
    void resetPool();

    // Diagnostic helpers (read-only)
    [[nodiscard]] uint32_t                                 getMaxSets() const { return maxSets; }
    [[nodiscard]] const std::vector<VkDescriptorPoolSize>& getPoolSizes() const { return poolSizes; }

  private:
    Device&          device;
    VkDescriptorPool descriptorPool;

    // Stored at creation time so we can emit helpful diagnostics when alloc fails
    std::vector<VkDescriptorPoolSize> poolSizes;
    uint32_t                          maxSets;
    VkDescriptorPoolCreateFlags       poolFlags;

    // Overflow/fallback pools owned by this DescriptorPool (prototype)
    mutable std::vector<VkDescriptorPool> overflowPools;
    mutable std::mutex                    overflowMutex;
    bool                                  allowOverflow = false;

    friend class DescriptorWriter;
  };

  class DescriptorWriter
  {
  public:
    DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);

    DescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
    DescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

    // Extended build: optional outResult supplies the underlying VkResult when
    // allocation fails (helps callers decide whether to grow pools, retry,
    // or fail fast). Backwards-compatible default keeps existing call sites OK.
    bool build(VkDescriptorSet& set, VkResult* outResult = nullptr);
    void overwrite(VkDescriptorSet& set);

  private:
    DescriptorSetLayout&              setLayout;
    DescriptorPool&                   pool;
    std::vector<VkWriteDescriptorSet> writes;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DESCRIPTORS_HPP
