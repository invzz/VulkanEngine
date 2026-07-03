#include <gtest/gtest.h>

#include "../../fixtures/DescriptorFixture.hpp"

using namespace engine;
using namespace engine::test;

class DescriptorPoolTest : public DescriptorFixture {};

TEST_F(DescriptorPoolTest, GivenPoolWithMaxSets_WhenAllocatingUpToLimit_ThenSucceeds) {
    auto layout = createUBOLayout(VK_SHADER_STAGE_FRAGMENT_BIT);
    auto pool   = createFreeablePool(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    VkDescriptorSet a = VK_NULL_HANDLE;
    VkDescriptorSet b = VK_NULL_HANDLE;

    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), b));
}

TEST_F(DescriptorPoolTest, GivenExhaustedPool_WhenAllocating_ThenFails) {
    auto layout = createUBOLayout(VK_SHADER_STAGE_FRAGMENT_BIT);
    auto pool   = createFreeablePool(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    VkDescriptorSet a = VK_NULL_HANDLE;
    VkDescriptorSet b = VK_NULL_HANDLE;
    VkDescriptorSet c = VK_NULL_HANDLE;

    pool->allocateDescriptor(layout->getDescriptorSetLayout(), a);
    pool->allocateDescriptor(layout->getDescriptorSetLayout(), b);

    EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), c));
}

TEST_F(DescriptorPoolTest, GivenExhaustedPool_WhenDescriptorsFreed_ThenCanAllocateAgain) {
    auto layout = createUBOLayout(VK_SHADER_STAGE_FRAGMENT_BIT);
    auto pool   = createFreeablePool(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    VkDescriptorSet a = VK_NULL_HANDLE;
    VkDescriptorSet b = VK_NULL_HANDLE;

    pool->allocateDescriptor(layout->getDescriptorSetLayout(), a);
    pool->allocateDescriptor(layout->getDescriptorSetLayout(), b);

    std::vector<VkDescriptorSet> toFree = {a, b};
    pool->freeDescriptors(toFree);

    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), a));
}

TEST_F(DescriptorPoolTest, GivenExhaustedPool_WhenReset_ThenCanReallocate) {
    auto layout = createUBOLayout(VK_SHADER_STAGE_FRAGMENT_BIT);
    auto pool   = createPool(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    std::vector<VkDescriptorSet> allocated(3);
    for (uint32_t i = 0; i < 3; ++i) {
        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), allocated[i]));
    }

    VkDescriptorSet extra = VK_NULL_HANDLE;
    EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), extra));

    pool->resetPool();

    EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), allocated[0]));
}

TEST_F(DescriptorPoolTest, GivenInsufficientPoolSize_WhenAllocating_ThenFails) {
    auto pool   = createPool(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

    VkDescriptorSet ds0 = VK_NULL_HANDLE;
    VkDescriptorSet ds1 = VK_NULL_HANDLE;

    bool ok0 = pool->allocateDescriptor(layout->getDescriptorSetLayout(), ds0);
    bool ok1 = pool->allocateDescriptor(layout->getDescriptorSetLayout(), ds1);

    EXPECT_TRUE(ok0);
    EXPECT_FALSE(ok1);
    EXPECT_EQ(ds1, VK_NULL_HANDLE);
}
