#include "Engine/Graphics/Descriptors.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {
  DescriptorSetLayout::Builder&
  DescriptorSetLayout::Builder::addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count, VkDescriptorBindingFlags flags)
  {
    assert(bindings.count(binding) == 0 && "Binding already in use");
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding         = binding;
    layoutBinding.descriptorType  = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags      = stageFlags;
    bindings[binding]             = layoutBinding;
    bindingFlags[binding]         = flags;
    return *this;
  }

  std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const
  {
    return std::make_unique<DescriptorSetLayout>(device, bindings, bindingFlags);
  }

  DescriptorSetLayout::DescriptorSetLayout(Device&                                                           device,
                                           const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings,
                                           const std::unordered_map<uint32_t, VkDescriptorBindingFlags>&     bindingFlags)
      : device{device}, bindings{bindings}
  {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    std::vector<VkDescriptorBindingFlags>     setLayoutBindingFlags{};

    // Sort bindings by binding index to ensure consistent order
    std::vector<uint32_t> keys;
    keys.reserve(bindings.size());
    for (const auto& [binding, _] : bindings)
    {
      keys.push_back(binding);
    }
    std::ranges::sort(keys);

    for (uint32_t const binding : keys)
    {
      setLayoutBindings.push_back(bindings.at(binding));
      if (bindingFlags.contains(binding) != 0u)
      {
        setLayoutBindingFlags.push_back(bindingFlags.at(binding));
      }
      else
      {
        setLayoutBindingFlags.push_back(0);
      }
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount  = static_cast<uint32_t>(setLayoutBindingFlags.size());
    bindingFlagsInfo.pBindingFlags = setLayoutBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings    = setLayoutBindings.data();
    descriptorSetLayoutInfo.pNext        = &bindingFlagsInfo;

    // Check if we need UPDATE_AFTER_BIND_POOL_BIT
    for (auto flag : setLayoutBindingFlags)
    {
      if ((flag & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0u)
      {
        descriptorSetLayoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        break;
      }
    }

    if (vkCreateDescriptorSetLayout(device.device(), &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create descriptor set layout!");
    }
  }

  DescriptorSetLayout::~DescriptorSetLayout()
  {
    vkDestroyDescriptorSetLayout(device.device(), descriptorSetLayout, nullptr);
  }

  DescriptorPool::Builder& DescriptorPool::Builder::addPoolSize(VkDescriptorType descriptorType, uint32_t count)
  {
    poolSizes.push_back({descriptorType, count});
    return *this;
  }

  DescriptorPool::Builder& DescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags)
  {
    poolFlags = flags;
    return *this;
  }

  DescriptorPool::Builder& DescriptorPool::Builder::setMaxSets(uint32_t count)
  {
    maxSets = count;
    return *this;
  }

  DescriptorPool::Builder& DescriptorPool::Builder::setAllowOverflow(bool allow)
  {
    allowOverflow = allow;
    return *this;
  }

  std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build() const
  {
    return std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes, allowOverflow);
  }

  DescriptorPool::DescriptorPool(Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes, bool allowOverflow)
      : device{device}, poolSizes{poolSizes}, maxSets{maxSets}, poolFlags{poolFlags}, allowOverflow{allowOverflow}
  {
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes    = poolSizes.data();
    descriptorPoolInfo.maxSets       = maxSets;
    descriptorPoolInfo.flags         = poolFlags;
    if (vkCreateDescriptorPool(device.device(), &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create descriptor pool!");
    }
  }

  DescriptorPool::~DescriptorPool()
  {
    // Destroy any overflow/fallback pools we created so they don't leak at device teardown.
    {
      std::lock_guard<std::mutex> lk(overflowMutex);
      for (auto p : overflowPools)
      {
        if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(device.device(), p, nullptr);
      }
      overflowPools.clear();
    }

    vkDestroyDescriptorPool(device.device(), descriptorPool, nullptr);
  }

  bool DescriptorPool::allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor, const std::vector<VkDescriptorPoolSize>* requestedPoolSizes)
  {
    // Try primary pool first
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.pSetLayouts        = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;
    VkResult result              = vkAllocateDescriptorSets(device.device(), &allocInfo, &descriptor);
    if (result == VK_SUCCESS)
    {
      return true;
    }

    // Primary allocation failed — emit diagnostics
    std::cerr << "vkAllocateDescriptorSets failed (result=" << result << ") on primary pool\n";
    std::cerr << "  pool.maxSets=" << maxSets << ", pool.flags=" << poolFlags << "\n";
    for (const auto& ps : poolSizes)
    {
      std::cerr << "  poolSize: type=" << ps.type << " count=" << ps.descriptorCount << "\n";
    }

    if (!allowOverflow)
    {
      if (result == VK_ERROR_FRAGMENTED_POOL)
        std::cerr << "  Suggestion: pool is fragmented; consider using resetPool() or creating a larger pool.\n";
      else if (result == VK_ERROR_OUT_OF_POOL_MEMORY)
        std::cerr << "  Suggestion: increase pool size for the descriptor type(s) in use.\n";
      return false;
    }

    // Overflow/fallback path (prototype): create a small transient pool sized
    // for the requested bindings and allocate from it. Store the pool so its
    // lifetime is tied to this DescriptorPool.
    std::vector<VkDescriptorPoolSize> fallbackSizes;
    if (requestedPoolSizes && !requestedPoolSizes->empty())
    {
      fallbackSizes = *requestedPoolSizes;
    }
    else
    {
      // Fallback to a conservative copy of the original poolSizes (at least 1)
      fallbackSizes = poolSizes;
      for (auto& ps : fallbackSizes)
        ps.descriptorCount = std::max<uint32_t>(1u, ps.descriptorCount);
      if (fallbackSizes.empty()) fallbackSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
    }

    VkDescriptorPoolCreateInfo fallbackInfo{};
    fallbackInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    fallbackInfo.poolSizeCount = static_cast<uint32_t>(fallbackSizes.size());
    fallbackInfo.pPoolSizes    = fallbackSizes.data();
    fallbackInfo.maxSets       = 1;
    fallbackInfo.flags         = poolFlags; // inherit flags

    VkDescriptorPool fallbackPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device.device(), &fallbackInfo, nullptr, &fallbackPool) != VK_SUCCESS)
    {
      std::cerr << "DescriptorPool: fallback pool creation failed\n";
      return false;
    }

    // Try allocation from fallback pool
    VkDescriptorSetAllocateInfo fallbackAlloc{};
    fallbackAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    fallbackAlloc.descriptorPool     = fallbackPool;
    fallbackAlloc.pSetLayouts        = &descriptorSetLayout;
    fallbackAlloc.descriptorSetCount = 1;

    VkDescriptorSet fallbackSet    = VK_NULL_HANDLE;
    VkResult        fallbackResult = vkAllocateDescriptorSets(device.device(), &fallbackAlloc, &fallbackSet);
    if (fallbackResult != VK_SUCCESS)
    {
      std::cerr << "DescriptorPool: allocation from fallback pool failed (result=" << fallbackResult << ")\n";
      vkDestroyDescriptorPool(device.device(), fallbackPool, nullptr);
      return false;
    }

    // Record fallback pool ownership so it isn't destroyed while its sets are live
    {
      std::lock_guard<std::mutex> lk(overflowMutex);
      overflowPools.push_back(fallbackPool);
    }

    descriptor = fallbackSet;
    std::cerr << "DescriptorPool: allocation succeeded from overflow pool (fallback).\n";
    return true;
  }

  void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
  {
    vkFreeDescriptorSets(device.device(), descriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
  }

  void DescriptorPool::resetPool()
  {
    // Reset primary pool and destroy any transient overflow pools to avoid
    // leaking descriptor pools when callers expect a clean reset.
    vkResetDescriptorPool(device.device(), descriptorPool, 0);
    std::lock_guard<std::mutex> lk(overflowMutex);
    for (auto p : overflowPools)
    {
      if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(device.device(), p, nullptr);
    }
    overflowPools.clear();
  }

  DescriptorWriter::DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool) : setLayout{setLayout}, pool{pool} {}

  DescriptorWriter& DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
  {
    assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
    const auto& bindingDescription = setLayout.bindings[binding];
    assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType  = bindingDescription.descriptorType;
    write.dstBinding      = binding;
    write.pBufferInfo     = bufferInfo;
    write.descriptorCount = 1;
    writes.push_back(write);
    return *this;
  }

  DescriptorWriter& DescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo)
  {
    assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
    const auto& bindingDescription = setLayout.bindings[binding];
    assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType  = bindingDescription.descriptorType;
    write.dstBinding      = binding;
    write.pImageInfo      = imageInfo;
    write.descriptorCount = 1;
    writes.push_back(write);
    return *this;
  }

  bool DescriptorWriter::build(VkDescriptorSet& set, VkResult* outResult)
  {
    // Defensive check BEFORE allocation: ensure that all bindings declared in
    // the layout have corresponding writes. Doing this check prior to
    // allocation avoids needing to free descriptor sets (which requires the
    // pool to be created with FREE_DESCRIPTOR_SET_BIT and would otherwise
    // trigger validation warnings).
    {
      std::unordered_set<uint32_t> writtenBindings;
      writtenBindings.reserve(writes.size());
      for (auto const& w : writes)
      {
        writtenBindings.insert(w.dstBinding);
      }

      for (const auto& [binding, _] : setLayout.bindings)
      {
        if (writtenBindings.count(binding) == 0)
        {
          // Missing a binding — don't allocate and signal failure. Provide a
          // clear diagnostic via outResult when caller requested it.
          set = VK_NULL_HANDLE;
          if (outResult) *outResult = VK_ERROR_INITIALIZATION_FAILED;
          std::cerr << "DescriptorWriter::build(): missing write for binding " << binding << "\n";
          return false;
        }
      }
    }

    if (bool const success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set); !success)
    {
      // allocateDescriptor already logged the VkResult; surface it to caller.
      if (outResult)
      {
        // Try to query the last VkResult by attempting a no-op allocation
        // is not possible here — allocateDescriptor logged the concrete
        // VkResult already. Set a generic non-success if caller asked.
        *outResult = VK_ERROR_OUT_OF_POOL_MEMORY;
      }
      return false;
    }

    overwrite(set);
    if (outResult) *outResult = VK_SUCCESS;
    return true;
  }

  void DescriptorWriter::overwrite(VkDescriptorSet& set)
  {
    for (auto& write : writes)
    {
      write.dstSet = set;
    }
    vkUpdateDescriptorSets(pool.device.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
  }
} // namespace engine
