/**
 * @file DescriptorFixture.hpp
 * @brief Shared test fixtures for descriptor-related tests
 *
 * Provides common setup for tests involving DescriptorPool, DescriptorSetLayout,
 * and DescriptorWriter. Reduces boilerplate for descriptor allocation tests.
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_DESCRIPTORFIXTURE_HPP
#define VULKANENGINE_TESTS_FIXTURES_DESCRIPTORFIXTURE_HPP

#include <gtest/gtest.h>
#include <memory>

#include "Engine/Graphics/Descriptors.hpp"

#include "DeviceFixture.hpp"

namespace engine::test {

    /**
 * @brief Base descriptor fixture with shared Device
 *
 * Provides the Device from DeviceFixture for descriptor tests.
 */
    class DescriptorFixture : public DeviceFixture {
       protected:
        // Helper to create a simple UBO layout (common pattern)
        std::unique_ptr<DescriptorSetLayout> createUBOLayout(VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL) {
            return DescriptorSetLayout::Builder(device()).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, stageFlags).build();
        }

        // Helper to create a combined image sampler layout
        std::unique_ptr<DescriptorSetLayout> createSamplerLayout(uint32_t bindingCount = 1, VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT) {
            auto builder = DescriptorSetLayout::Builder(device());
            for (uint32_t i = 0; i < bindingCount; ++i) {
                builder.addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stageFlags);
            }
            return builder.build();
        }

        // Helper to create a pool with common settings
        std::unique_ptr<DescriptorPool> createPool(uint32_t maxSets, VkDescriptorType type, VkDescriptorPoolCreateFlags flags = 0) {
            return DescriptorPool::Builder(device()).setMaxSets(maxSets).addPoolSize(type, maxSets).setPoolFlags(flags).build();
        }

        // Helper to create a freeable pool (can free individual descriptors)
        std::unique_ptr<DescriptorPool> createFreeablePool(uint32_t maxSets, VkDescriptorType type) {
            return createPool(maxSets, type, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
        }
    };

    /**
 * @brief Descriptor fixture with pre-created pool and layout
 *
 * Provides a ready-to-use pool and layout for allocation tests.
 */
    class DescriptorPoolFixture : public DescriptorFixture {
       protected:
        void SetUp() override {
            layout_ = createUBOLayout();
            pool_   = createFreeablePool(kDefaultMaxSets, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        }

        void TearDown() override {
            pool_.reset();
            layout_.reset();
        }

        DescriptorSetLayout& layout() {
            return *layout_;
        }
        DescriptorPool& pool() {
            return *pool_;
        }

        VkDescriptorSetLayout layoutHandle() {
            return layout_->getDescriptorSetLayout();
        }

        // Allocate a descriptor set from the pool
        VkDescriptorSet allocateSet() {
            VkDescriptorSet set = VK_NULL_HANDLE;
            pool_->allocateDescriptor(layoutHandle(), set);
            return set;
        }

        static constexpr uint32_t kDefaultMaxSets = 10;

       private:
        std::unique_ptr<DescriptorSetLayout> layout_;
        std::unique_ptr<DescriptorPool>      pool_;
    };

}  // namespace engine::test

#endif  // VULKANENGINE_TESTS_FIXTURES_DESCRIPTORFIXTURE_HPP
