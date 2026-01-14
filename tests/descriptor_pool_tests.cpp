#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

TEST(DescriptorPool, AllocateUpToMaxSetsThenFail)
{
  Window window(1, 1, "DescriptorPoolTestWindow");
  Device device(window);

  // Layout: single dynamic uniform buffer binding
  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 2;
  auto           pool =
          DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

  VkDescriptorSet a = VK_NULL_HANDLE;
  VkDescriptorSet b = VK_NULL_HANDLE;
  VkDescriptorSet c = VK_NULL_HANDLE;

  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), b));

  // Third allocation should fail (pool exhausted)
  EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), c));

  // Free and try again
  std::vector<VkDescriptorSet> toFree = {a, b};
  pool->freeDescriptors(toFree);

  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
}

TEST(DescriptorPool, ResetPoolAllowsReallocation)
{
  Window window(1, 1, "DescriptorPoolTestWindow2");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  const uint32_t maxSets = 3;
  auto           pool    = DescriptorPool::Builder(device).setMaxSets(maxSets).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets).build();

  std::vector<VkDescriptorSet> allocated;
  allocated.resize(maxSets);
  for (uint32_t i = 0; i < maxSets; ++i)
  {
    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), allocated[i]));
  }

  VkDescriptorSet extra = VK_NULL_HANDLE;
  EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), extra));

  // Reset pool and ensure we can allocate again
  pool->resetPool();

  EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), extra));
}
