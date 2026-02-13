#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

// =============================================================================
// DescriptorWriter Tests
// =============================================================================

TEST(DescriptorWriter, GivenExhaustedPool_WhenBuildCalled_ThenReturnsFalse) {
  Window window(1, 1, "DescriptorWriterTest");
  Device device(window);

  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build();

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

  // Create a buffer for descriptor writes
  VkBuffer buf;
  VkDeviceMemory bufMem;
  device.getMemory().createBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, bufMem);
  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = buf;
  bufInfo.offset = 0;
  bufInfo.range = 256;

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

TEST(DescriptorWriter, GivenPoolWithOverflowEnabled_WhenPoolExhausted_ThenFallbackSucceeds) {
  Window window(1, 1, "DescriptorWriterTest");
  Device device(window);

  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).setAllowOverflow(true).build();

  auto layout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

  VkBuffer buf;
  VkDeviceMemory bufMem;
  device.getMemory().createBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, bufMem);
  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = buf;
  bufInfo.offset = 0;
  bufInfo.range = 256;

  VkDescriptorSet ds0 = VK_NULL_HANDLE;
  VkDescriptorSet ds1 = VK_NULL_HANDLE;

  VkResult r0 = VK_SUCCESS;
  bool ok0 = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufInfo).build(ds0, &r0);
  ASSERT_TRUE(ok0);
  ASSERT_EQ(r0, VK_SUCCESS);
  ASSERT_NE(ds0, VK_NULL_HANDLE);

  // Second allocation should succeed via overflow fallback
  VkResult r1 = VK_SUCCESS;
  bool ok1 = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufInfo).build(ds1, &r1);
  EXPECT_TRUE(ok1);
  EXPECT_EQ(r1, VK_SUCCESS);
  EXPECT_NE(ds1, VK_NULL_HANDLE);
  EXPECT_NE(ds0, ds1);

  vkDestroyBuffer(device.device(), buf, nullptr);
  vkFreeMemory(device.device(), bufMem, nullptr);
}

TEST(DescriptorWriter, GivenLayoutWithMultipleBindings_WhenBindingMissing_ThenBuildFails) {
  Window window(1, 1, "DescriptorWriterTest");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device)
                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build();

  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2).build();

  VkDescriptorSet ds = VK_NULL_HANDLE;
  VkDescriptorImageInfo dummy{};

  // Only write binding 0, leave binding 1 missing — build() must fail
  VkResult result = VK_SUCCESS;
  bool ok = DescriptorWriter(*layout, *pool).writeImage(0, &dummy).build(ds, &result);

  EXPECT_FALSE(ok);
  EXPECT_EQ(ds, VK_NULL_HANDLE);
  EXPECT_EQ(result, VK_ERROR_INITIALIZATION_FAILED);
}
