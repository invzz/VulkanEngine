#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

TEST(DescriptorWriter, BuildSurfacesAllocationFailure)
{
  Window window(1, 1, "DWAFT");
  Device device(window);

  // Create a pool limited to a single set so the second allocation fails.
  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build();

  auto layout = DescriptorSetLayout::Builder(device)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                        .build();

  // Create a tiny host-visible buffer to use as a valid descriptor payload so
  // vkUpdateDescriptorSets won't trip validation for null imageView/sampler.
  VkBuffer       buf;
  VkDeviceMemory bufMem;
  device.getMemory().createBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, bufMem);
  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = buf;
  bufInfo.offset = 0;
  bufInfo.range  = 256;

  VkDescriptorSet ds0 = VK_NULL_HANDLE;
  VkDescriptorSet ds1 = VK_NULL_HANDLE;

  VkResult result0 = VK_SUCCESS;
  bool ok0 = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufInfo).build(ds0, &result0);
  EXPECT_TRUE(ok0);
  EXPECT_EQ(result0, VK_SUCCESS);
  EXPECT_NE(ds0, VK_NULL_HANDLE);

  VkResult result1 = VK_SUCCESS;
  bool ok1 = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufInfo).build(ds1, &result1);

  EXPECT_FALSE(ok1);
  EXPECT_NE(result1, VK_SUCCESS);
  EXPECT_EQ(ds1, VK_NULL_HANDLE);

  vkDestroyBuffer(device.device(), buf, nullptr);
  vkFreeMemory(device.device(), bufMem, nullptr);
}
