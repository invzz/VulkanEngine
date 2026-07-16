#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DESCRIPTORS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DESCRIPTORS_HPP
#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Device.hpp"
namespace engine {
    class DescriptorSetLayout {
       public:
        class Builder {
           public:
            explicit Builder(Device& device) : device{device} {}
            Builder&                                           addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1, VkDescriptorBindingFlags bindingFlags = 0);
            [[nodiscard]] std::unique_ptr<DescriptorSetLayout> build() const;

           private:
            Device&                                                    device;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
            std::unordered_map<uint32_t, VkDescriptorBindingFlags>     bindingFlags;
        };
        DescriptorSetLayout(Device& device, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings, const std::unordered_map<uint32_t, VkDescriptorBindingFlags>& bindingFlags);
        ~DescriptorSetLayout();
        DescriptorSetLayout(const DescriptorSetLayout&)                           = delete;
        DescriptorSetLayout&                operator=(const DescriptorSetLayout&) = delete;
        [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const {
            return descriptorSetLayout;
        }

       private:
        Device&                                                    device;
        VkDescriptorSetLayout                                      descriptorSetLayout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        friend class DescriptorWriter;
    };
    class DescriptorPool {
       public:
        class Builder {
           public:
            Builder(Device& device) : device{device} {}
            Builder&                                      addPoolSize(VkDescriptorType descriptorType, uint32_t count);
            Builder&                                      setPoolFlags(VkDescriptorPoolCreateFlags flags);
            Builder&                                      setMaxSets(uint32_t count);
            Builder&                                      setAllowOverflow(bool allow);
            Builder&                                      setRequireSuccess(bool require);
            [[nodiscard]] std::unique_ptr<DescriptorPool> build() const;

           private:
            Device&                           device;
            std::vector<VkDescriptorPoolSize> poolSizes;
            uint32_t                          maxSets        = 1000;
            VkDescriptorPoolCreateFlags       poolFlags      = 0;
            bool                              allowOverflow  = false;
            bool                              requireSuccess = true;
        };
        DescriptorPool(Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes, bool allowOverflow = false);
        ~DescriptorPool();
        DescriptorPool(const DescriptorPool&)                   = delete;
        DescriptorPool&        operator=(const DescriptorPool&) = delete;
        bool                   allocateDescriptor(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor, const std::vector<VkDescriptorPoolSize>* requestedPoolSizes = nullptr);
        void                   freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;
        void                   resetPool();
        [[nodiscard]] uint32_t getMaxSets() const {
            return maxSets;
        }
        [[nodiscard]] const std::vector<VkDescriptorPoolSize>& getPoolSizes() const {
            return poolSizes;
        }

       private:
        Device&                               device;
        VkDescriptorPool                      descriptorPool;
        std::vector<VkDescriptorPoolSize>     poolSizes;
        uint32_t                              maxSets;
        VkDescriptorPoolCreateFlags           poolFlags;
        mutable std::vector<VkDescriptorPool> overflowPools;
        mutable std::mutex                    overflowMutex;
        bool                                  allowOverflow = false;
        friend class DescriptorWriter;
    };
    class DescriptorWriter {
       public:
        DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);
        DescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        DescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);
        DescriptorWriter& writeImageArray(uint32_t binding, VkDescriptorImageInfo* imageInfos, uint32_t count);
        DescriptorWriter& writeAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR accel);
        bool              build(VkDescriptorSet& set, VkResult* outResult = nullptr);
        void              buildOrThrow(VkDescriptorSet& set);
        void              overwrite(VkDescriptorSet& set);

       private:
        DescriptorSetLayout&              setLayout;
        DescriptorPool&                   pool;
        std::vector<VkWriteDescriptorSet> writes;
        // Parallel to writes: non-empty entries carry the pNext for accel writes
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelWrites;
        // Stable storage for accel handles pointed to by accelWrites entries
        std::vector<VkAccelerationStructureKHR> accelStructHandles_;
    };
}  // namespace engine
#endif
