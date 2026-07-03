/**
 * @file DescriptorsTests.cpp
 * @brief Unit tests for Descriptor classes (DescriptorSetLayout, DescriptorPool, DescriptorWriter)
 */

#include <gtest/gtest.h>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"

#include "../../fixtures/DeviceFixture.hpp"

namespace engine::test {

    class DescriptorsTest : public DeviceFixture {};

    TEST_F(DescriptorsTest, LayoutBuilder_SingleBinding) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, LayoutBuilder_MultipleBindings) {
        auto layout = DescriptorSetLayout::Builder(device())
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                          .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                          .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, LayoutBuilder_NonSequentialBindings) {
        auto layout = DescriptorSetLayout::Builder(device())
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                          .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                          .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, LayoutBuilder_WithArrayCount) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4).build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, LayoutBuilder_AllShaderStages) {
        VkShaderStageFlags allStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, allStages).build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, LayoutBuilder_WithBindingFlags) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT).build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, PoolBuilder_SinglePoolSize) {
        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        ASSERT_NE(pool, nullptr);
        EXPECT_EQ(pool->getMaxSets(), 10u);
        EXPECT_EQ(pool->getPoolSizes().size(), 1u);
    }

    TEST_F(DescriptorsTest, PoolBuilder_MultiplePoolSizes) {
        auto pool = DescriptorPool::Builder(device())
                        .setMaxSets(100)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 25)
                        .build();

        ASSERT_NE(pool, nullptr);
        EXPECT_EQ(pool->getMaxSets(), 100u);
        EXPECT_EQ(pool->getPoolSizes().size(), 3u);
    }

    TEST_F(DescriptorsTest, PoolBuilder_WithFlags) {
        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

        ASSERT_NE(pool, nullptr);
        EXPECT_EQ(pool->getMaxSets(), 10u);
    }

    TEST_F(DescriptorsTest, PoolBuilder_AllowOverflow) {
        auto pool = DescriptorPool::Builder(device()).setMaxSets(5).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5).setAllowOverflow(true).build();

        ASSERT_NE(pool, nullptr);
        EXPECT_EQ(pool->getMaxSets(), 5u);
    }

    TEST_F(DescriptorsTest, PoolBuilder_DefaultMaxSets) {
        auto pool = DescriptorPool::Builder(device()).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000).build();

        ASSERT_NE(pool, nullptr);

        EXPECT_EQ(pool->getMaxSets(), 1000u);
    }

    TEST_F(DescriptorsTest, Pool_AllocateSingleDescriptor) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        bool            result        = pool->allocateDescriptor(layout->getDescriptorSetLayout(), descriptorSet);

        EXPECT_TRUE(result);
        EXPECT_NE(descriptorSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Pool_AllocateMultipleDescriptors) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        std::vector<VkDescriptorSet> descriptorSets(5, VK_NULL_HANDLE);
        for (auto& set : descriptorSets) {
            EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set));
            EXPECT_NE(set, VK_NULL_HANDLE);
        }

        std::set<VkDescriptorSet> uniqueSets(descriptorSets.begin(), descriptorSets.end());
        EXPECT_EQ(uniqueSets.size(), descriptorSets.size());
    }

    TEST_F(DescriptorsTest, Pool_ResetPool) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(3).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3).build();

        for (int i = 0; i < 3; ++i) {
            VkDescriptorSet set = VK_NULL_HANDLE;
            EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set));
        }

        pool->resetPool();

        VkDescriptorSet newSet = VK_NULL_HANDLE;
        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), newSet));
        EXPECT_NE(newSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Pool_FreeDescriptors) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(5).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

        std::vector<VkDescriptorSet> sets(3, VK_NULL_HANDLE);
        for (auto& set : sets) {
            EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set));
        }

        pool->freeDescriptors(sets);

        VkDescriptorSet newSet = VK_NULL_HANDLE;
        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), newSet));
    }

    TEST_F(DescriptorsTest, Writer_WriteBuffer) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        Buffer buffer(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer.getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range  = buffer.getBufferSize();

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        bool            result        = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufferInfo).build(descriptorSet);

        EXPECT_TRUE(result);
        EXPECT_NE(descriptorSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Writer_BuildWithOutResult) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        Buffer buffer(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer.getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range  = buffer.getBufferSize();

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkResult        vkResult      = VK_ERROR_UNKNOWN;

        bool result = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufferInfo).build(descriptorSet, &vkResult);

        EXPECT_TRUE(result);
        EXPECT_EQ(vkResult, VK_SUCCESS);
        EXPECT_NE(descriptorSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Writer_Overwrite) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10).build();

        Buffer buffer1(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        Buffer buffer2(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDescriptorBufferInfo bufferInfo1{};
        bufferInfo1.buffer = buffer1.getBuffer();
        bufferInfo1.offset = 0;
        bufferInfo1.range  = buffer1.getBufferSize();

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        DescriptorWriter(*layout, *pool).writeBuffer(0, &bufferInfo1).build(descriptorSet);

        ASSERT_NE(descriptorSet, VK_NULL_HANDLE);

        VkDescriptorBufferInfo bufferInfo2{};
        bufferInfo2.buffer = buffer2.getBuffer();
        bufferInfo2.offset = 0;
        bufferInfo2.range  = buffer2.getBufferSize();

        DescriptorWriter(*layout, *pool).writeBuffer(0, &bufferInfo2).overwrite(descriptorSet);

        EXPECT_NE(descriptorSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Writer_MultipleBindings) {
        auto layout = DescriptorSetLayout::Builder(device())
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                          .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                          .build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(10).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20).build();

        Buffer buffer1(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        Buffer buffer2(device(), sizeof(float) * 16, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDescriptorBufferInfo bufferInfo1{};
        bufferInfo1.buffer = buffer1.getBuffer();
        bufferInfo1.offset = 0;
        bufferInfo1.range  = buffer1.getBufferSize();

        VkDescriptorBufferInfo bufferInfo2{};
        bufferInfo2.buffer = buffer2.getBuffer();
        bufferInfo2.offset = 0;
        bufferInfo2.range  = buffer2.getBufferSize();

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        bool            result        = DescriptorWriter(*layout, *pool).writeBuffer(0, &bufferInfo1).writeBuffer(1, &bufferInfo2).build(descriptorSet);

        EXPECT_TRUE(result);
        EXPECT_NE(descriptorSet, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Pool_ExhaustionWithoutOverflow) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(2).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2).setAllowOverflow(false).build();

        VkDescriptorSet set1 = VK_NULL_HANDLE, set2 = VK_NULL_HANDLE, set3 = VK_NULL_HANDLE;

        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set1));
        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set2));

        EXPECT_FALSE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set3));
    }

    TEST_F(DescriptorsTest, Pool_ExhaustionWithOverflow) {
        auto layout = DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build();

        auto pool = DescriptorPool::Builder(device()).setMaxSets(2).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2).setAllowOverflow(true).build();

        VkDescriptorSet set1 = VK_NULL_HANDLE, set2 = VK_NULL_HANDLE, set3 = VK_NULL_HANDLE;

        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set1));
        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set2));

        EXPECT_TRUE(pool->allocateDescriptor(layout->getDescriptorSetLayout(), set3));
        EXPECT_NE(set3, VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Layout_EmptyLayout) {
        auto layout = DescriptorSetLayout::Builder(device()).build();

        ASSERT_NE(layout, nullptr);
        EXPECT_NE(layout->getDescriptorSetLayout(), VK_NULL_HANDLE);
    }

    TEST_F(DescriptorsTest, Pool_AllDescriptorTypes) {
        auto pool = DescriptorPool::Builder(device())
                        .setMaxSets(100)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10)
                        .build();

        ASSERT_NE(pool, nullptr);
        EXPECT_EQ(pool->getPoolSizes().size(), 11u);
    }

}  // namespace engine::test
