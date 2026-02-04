#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

// =============================================================================
// DescriptorPool Tests
// =============================================================================

TEST(DescriptorPool, GivenPoolWithMaxSets_WhenAllocatingUpToLimit_ThenSucceeds)
{
  Window window(1, 1, "DescriptorPoolTest");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 2;
  auto           pool =
          DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

  VkDescriptorSet a = VK_NULL_HANDLE;
  VkDescriptorSet b = VK_NULL_HANDLE;

  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), b));
}

TEST(DescriptorPool, GivenExhaustedPool_WhenAllocating_ThenFails)
{
  Window window(1, 1, "DescriptorPoolTest");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 2;
  auto           pool =
          DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

  VkDescriptorSet a = VK_NULL_HANDLE;
  VkDescriptorSet b = VK_NULL_HANDLE;
  VkDescriptorSet c = VK_NULL_HANDLE;

  pool->allocateDescriptor(layout->getDescriptorSetLayout(), a);
  pool->allocateDescriptor(layout->getDescriptorSetLayout(), b);

  // Third allocation should fail (pool exhausted)
  EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), c));
}

TEST(DescriptorPool, GivenExhaustedPool_WhenDescriptorsFreed_ThenCanAllocateAgain)
{
  Window window(1, 1, "DescriptorPoolTest");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 2;
  auto           pool =
          DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

  VkDescriptorSet a = VK_NULL_HANDLE;
  VkDescriptorSet b = VK_NULL_HANDLE;

  pool->allocateDescriptor(layout->getDescriptorSetLayout(), a);
  pool->allocateDescriptor(layout->getDescriptorSetLayout(), b);

  std::vector<VkDescriptorSet> toFree = {a, b};
  pool->freeDescriptors(toFree);

  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
}

TEST(DescriptorPool, GivenExhaustedPool_WhenReset_ThenCanReallocate)
{
  Window window(1, 1, "DescriptorPoolTest");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 3;
  auto           pool    = DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).build();

  std::vector<VkDescriptorSet> allocated(maxSets);
  for (uint32_t i = 0; i < maxSets; ++i)
  {
    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), allocated[i]));
  }

  VkDescriptorSet extra = VK_NULL_HANDLE;
  EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), extra));

  pool->resetPool();

  // After reset, should be able to allocate again
  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), allocated[0]));
}

TEST(DescriptorPool, GivenInsufficientPoolSize_WhenAllocating_ThenFails)
{
  Window window(1, 1, "DescriptorPoolTest");
  Device device(window);

  // Make maxSets=1 so the second allocation must fail
  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2).build();

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

  VkDescriptorSet ds0 = VK_NULL_HANDLE;
  VkDescriptorSet ds1 = VK_NULL_HANDLE;

  bool ok0 = pool->allocateDescriptor(layout->getDescriptorSetLayout(), ds0);
  bool ok1 = pool->allocateDescriptor(layout->getDescriptorSetLayout(), ds1);

  EXPECT_TRUE(ok0);
  EXPECT_FALSE(ok1);
  EXPECT_EQ(ds1, VK_NULL_HANDLE);
}
