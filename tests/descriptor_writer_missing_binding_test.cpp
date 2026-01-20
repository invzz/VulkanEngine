#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

TEST(DescriptorWriter, BuildFailsIfMissingBindings)
{
  Window window(1, 1, "DW_Test");
  Device device(window);

  auto layout = DescriptorSetLayout::Builder(device)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .build();
  auto pool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2).build();

  VkDescriptorSet       ds = VK_NULL_HANDLE;
  VkDescriptorImageInfo dummy{}; // intentionally left empty; we only test missing binding behavior

  // Only write binding 0, leave binding 1 missing — build() must fail and
  // return a helpful VkResult when requested.
  VkResult result = VK_SUCCESS;
  bool const ok   = DescriptorWriter(*layout, *pool).writeImage(0, &dummy).build(ds, &result);
  EXPECT_FALSE(ok);
  EXPECT_EQ(ds, VK_NULL_HANDLE);
  EXPECT_EQ(result, VK_ERROR_INITIALIZATION_FAILED);
}
