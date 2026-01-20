#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

// Intentionally create a small pool and attempt allocations that exceed a
// particular poolSize to assert the allocation path fails cleanly.
TEST(DescriptorPool, AllocationFailsWhenPoolSizesInsufficient)
{
  Window window(1, 1, "DPF");
  Device device(window);

  // Make the test robust across drivers: limit maxSets to 1 and request two
  // descriptor sets — the second allocation must fail on all conformant
  // implementations.
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
