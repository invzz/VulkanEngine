#include "Engine/Graphics/Descriptors.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Engine/Core/ErrorCodes.hpp"
#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"

#include "vulkan/vulkan_core.h"

namespace engine {

    namespace {

        bool shouldInjectDescriptorAllocationFailure() {
            const char* env = std::getenv("ENGINE_INJECT_ALLOC_FAILURE");
            if (env == nullptr) {
                return false;
            }
            std::string token(env);
            return token.find("all") != std::string::npos || token.find("descriptor") != std::string::npos;
        }

    }  // namespace

    DescriptorSetLayout::Builder&
    DescriptorSetLayout::Builder::addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count, VkDescriptorBindingFlags flags) {
        assert(!bindings.contains(binding) && "Binding already in use");
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding         = binding;
        layoutBinding.descriptorType  = descriptorType;
        layoutBinding.descriptorCount = count;
        layoutBinding.stageFlags      = stageFlags;
        bindings[binding]             = layoutBinding;
        bindingFlags[binding]         = flags;
        return *this;
    }

    std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const {
        return std::make_unique<DescriptorSetLayout>(device, bindings, bindingFlags);
    }

    DescriptorSetLayout::DescriptorSetLayout(Device&                      device,
        const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings,
        const std::unordered_map<uint32_t, VkDescriptorBindingFlags>&     bindingFlags)
        : device{device}, bindings{bindings} {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        std::vector<VkDescriptorBindingFlags>     setLayoutBindingFlags{};

        std::vector<uint32_t> keys;
        keys.reserve(bindings.size());
        for (const auto& [binding, _] : bindings) {
            keys.push_back(binding);
        }
        std::ranges::sort(keys);

        for (uint32_t const binding : keys) {
            setLayoutBindings.push_back(bindings.at(binding));
            if (bindingFlags.contains(binding)) {
                setLayoutBindingFlags.push_back(bindingFlags.at(binding));
            } else {
                setLayoutBindingFlags.push_back(0);
            }
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount  = static_cast<uint32_t>(setLayoutBindingFlags.size());
        bindingFlagsInfo.pBindingFlags = setLayoutBindingFlags.data();

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings    = setLayoutBindings.data();
        descriptorSetLayoutInfo.pNext        = &bindingFlagsInfo;

        for (auto flag : setLayoutBindingFlags) {
            if ((flag & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0u) {
                descriptorSetLayoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                break;
            }
        }

        if (vkCreateDescriptorSetLayout(device.device(), &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create descriptor set layout!");
        }
    }

    DescriptorSetLayout::~DescriptorSetLayout() {
        vkDestroyDescriptorSetLayout(device.device(), descriptorSetLayout, nullptr);
    }

    DescriptorPool::Builder& DescriptorPool::Builder::addPoolSize(VkDescriptorType descriptorType, uint32_t count) {
        poolSizes.push_back({descriptorType, count});
        return *this;
    }

    DescriptorPool::Builder& DescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags) {
        poolFlags = flags;
        return *this;
    }

    DescriptorPool::Builder& DescriptorPool::Builder::setMaxSets(uint32_t count) {
        maxSets = count;
        return *this;
    }

    DescriptorPool::Builder& DescriptorPool::Builder::setAllowOverflow(bool allow) {
        allowOverflow = allow;
        return *this;
    }

    DescriptorPool::Builder& DescriptorPool::Builder::setRequireSuccess(bool require) {
        requireSuccess = require;
        return *this;
    }

    std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build() const {
        auto pool = std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes, allowOverflow);
        if (requireSuccess && pool->descriptorPool == VK_NULL_HANDLE) {
            throw std::runtime_error("DescriptorPool::Builder::build failed to create pool");
        }
        return pool;
    }

    DescriptorPool::DescriptorPool(Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes, bool allowOverflow)
        : device{device}, poolSizes{poolSizes}, maxSets{maxSets}, poolFlags{poolFlags}, allowOverflow{allowOverflow} {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes    = poolSizes.data();
        descriptorPoolInfo.maxSets       = maxSets;
        descriptorPoolInfo.flags         = poolFlags;
        if (vkCreateDescriptorPool(device.device(), &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            ErrorState::report(ErrorCode::DescriptorPoolCreationError, ErrorBoundary::Fatal, "Failed to create descriptor pool");
            Logger::error(LogChannel::Resource, "Failed to create descriptor pool (maxSets=", maxSets, ")");
            throw FatalGraphicsException(ErrorCode::DescriptorPoolCreationError, "failed to create descriptor pool!");
        }
    }

    DescriptorPool::~DescriptorPool() {
        {
            std::lock_guard<std::mutex> lk(overflowMutex);
            for (auto p : overflowPools) {
                if (p != VK_NULL_HANDLE)
                    vkDestroyDescriptorPool(device.device(), p, nullptr);
            }
            overflowPools.clear();
        }

        vkDestroyDescriptorPool(device.device(), descriptorPool, nullptr);
    }

    bool DescriptorPool::allocateDescriptor(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor, const std::vector<VkDescriptorPoolSize>* requestedPoolSizes) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool;
        allocInfo.pSetLayouts        = &descriptorSetLayout;
        allocInfo.descriptorSetCount = 1;
        VkResult result              = VK_ERROR_OUT_OF_POOL_MEMORY;
        if (shouldInjectDescriptorAllocationFailure()) {
            result = VK_ERROR_OUT_OF_POOL_MEMORY;
        } else {
            result = vkAllocateDescriptorSets(device.device(), &allocInfo, &descriptor);
        }
        if (result == VK_SUCCESS) {
            return true;
        }

        engine::Logger::error(engine::LogChannel::Resource, "vkAllocateDescriptorSets failed (result=", result, ") on primary pool");
        engine::Logger::error(engine::LogChannel::Resource, "  pool.maxSets=", maxSets, ", pool.flags=", poolFlags);
        for (const auto& ps : poolSizes) {
            engine::Logger::error(engine::LogChannel::Resource, "  poolSize: type=", ps.type, " count=", ps.descriptorCount);
        }

        if (!allowOverflow) {
            ErrorState::report(
                ErrorCode::DescriptorPoolAllocationError,
                ErrorBoundary::Fatal,
                std::string("Descriptor allocation failed without overflow (VkResult=") + std::to_string(result) + ")");
            Logger::error(LogChannel::Resource, "Descriptor allocation failed without overflow (VkResult=", result, ")");
            if (result == VK_ERROR_FRAGMENTED_POOL) {
                engine::Logger::warn(engine::LogChannel::Resource, "  Suggestion: pool is fragmented; consider using resetPool() or creating a larger pool.");
            } else if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
                engine::Logger::warn(engine::LogChannel::Resource, "  Suggestion: increase pool size for the descriptor type(s) in use.");
            }
            return false;
        }

        std::vector<VkDescriptorPoolSize> fallbackSizes;
        if (requestedPoolSizes != nullptr && !requestedPoolSizes->empty()) {
            fallbackSizes = *requestedPoolSizes;
        } else {
            fallbackSizes = poolSizes;
            for (auto& ps : fallbackSizes) {
                ps.descriptorCount = std::max<uint32_t>(1u, ps.descriptorCount);
            }
            if (fallbackSizes.empty()) {
                fallbackSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
            }
        }

        VkDescriptorPoolCreateInfo fallbackInfo{};
        fallbackInfo.sType            = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        fallbackInfo.poolSizeCount    = static_cast<uint32_t>(fallbackSizes.size());
        fallbackInfo.pPoolSizes       = fallbackSizes.data();
        fallbackInfo.maxSets          = 1;
        fallbackInfo.flags            = poolFlags;
        VkDescriptorPool fallbackPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device.device(), &fallbackInfo, nullptr, &fallbackPool) != VK_SUCCESS) {
            ErrorState::report(ErrorCode::DescriptorPoolCreationError, ErrorBoundary::Fatal, "Descriptor overflow pool creation failed");
            Logger::error(LogChannel::Resource, "Descriptor overflow pool creation failed");
            engine::Logger::error(engine::LogChannel::Resource, "DescriptorPool: fallback pool creation failed");
            return false;
        }

        VkDescriptorSetAllocateInfo fallbackAlloc{};
        fallbackAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        fallbackAlloc.descriptorPool     = fallbackPool;
        fallbackAlloc.pSetLayouts        = &descriptorSetLayout;
        fallbackAlloc.descriptorSetCount = 1;

        VkDescriptorSet fallbackSet    = VK_NULL_HANDLE;
        VkResult        fallbackResult = vkAllocateDescriptorSets(device.device(), &fallbackAlloc, &fallbackSet);
        if (fallbackResult != VK_SUCCESS) {
            ErrorState::report(
                ErrorCode::DescriptorPoolAllocationError,
                ErrorBoundary::Fatal,
                std::string("Descriptor allocation from overflow pool failed (VkResult=") + std::to_string(fallbackResult) + ")");
            Logger::error(LogChannel::Resource, "Descriptor allocation from overflow pool failed (VkResult=", fallbackResult, ")");
            engine::Logger::error(engine::LogChannel::Resource, "Descriptor allocation from overflow pool failed (result=", fallbackResult, ")");
            vkDestroyDescriptorPool(device.device(), fallbackPool, nullptr);
            return false;
        }

        {
            std::lock_guard<std::mutex> lk(overflowMutex);
            overflowPools.push_back(fallbackPool);
        }

        descriptor = fallbackSet;
        ErrorState::report(ErrorCode::DescriptorPoolOverflowUsed, ErrorBoundary::Recoverable, "Descriptor overflow pool used");
        Logger::warn(LogChannel::Resource, "Descriptor allocation succeeded from overflow pool (fallback)");
        engine::Logger::warn(engine::LogChannel::Resource, "Descriptor allocation succeeded from overflow pool (fallback).");
        return true;
    }

    void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const {
        vkFreeDescriptorSets(device.device(), descriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
    }

    void DescriptorPool::resetPool() {
        vkResetDescriptorPool(device.device(), descriptorPool, 0);
        std::lock_guard<std::mutex> lk(overflowMutex);
        for (auto p : overflowPools) {
            if (p != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device.device(), p, nullptr);
        }
        overflowPools.clear();
    }

    DescriptorWriter::DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool) : setLayout{setLayout}, pool{pool} {}

    DescriptorWriter& DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        const auto& bindingDescription = setLayout.bindings[binding];
        assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType  = bindingDescription.descriptorType;
        write.dstBinding      = binding;
        write.pBufferInfo     = bufferInfo;
        write.descriptorCount = 1;
        writes.push_back(write);
        return *this;
    }

    DescriptorWriter& DescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        const auto& bindingDescription = setLayout.bindings[binding];
        assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType  = bindingDescription.descriptorType;
        write.dstBinding      = binding;
        write.pImageInfo      = imageInfo;
        write.descriptorCount = 1;
        writes.push_back(write);
        return *this;
    }

    DescriptorWriter& DescriptorWriter::writeImageArray(uint32_t binding, VkDescriptorImageInfo* imageInfos, uint32_t count) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        const auto& bindingDescription = setLayout.bindings[binding];
        assert(bindingDescription.descriptorCount == count && "Array count does not match binding descriptorCount");
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType  = bindingDescription.descriptorType;
        write.dstBinding      = binding;
        write.pImageInfo      = imageInfos;
        write.descriptorCount = count;
        writes.push_back(write);
        return *this;
    }

    bool DescriptorWriter::build(VkDescriptorSet& set, VkResult* outResult) {
        {
            std::unordered_set<uint32_t> writtenBindings;
            writtenBindings.reserve(writes.size());
            for (auto const& w : writes) {
                writtenBindings.insert(w.dstBinding);
            }

            for (const auto& [binding, _] : setLayout.bindings) {
                if (!writtenBindings.contains(binding)) {
                    set = VK_NULL_HANDLE;
                    ErrorState::report(
                        ErrorCode::DescriptorPoolAllocationError,
                        ErrorBoundary::Fatal,
                        std::string("DescriptorWriter missing write for binding ") + std::to_string(binding));
                    Logger::error(LogChannel::Resource, "DescriptorWriter missing write for binding ", binding);
                    if (outResult != nullptr) {
                        *outResult = VK_ERROR_INITIALIZATION_FAILED;
                    }
                    return false;
                }
            }
        }

        if (bool const success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set); !success) {
            if (outResult != nullptr) {
                *outResult = VK_ERROR_OUT_OF_POOL_MEMORY;
            }
            return false;
        }

        overwrite(set);
        if (outResult != nullptr) {
            *outResult = VK_SUCCESS;
        }
        return true;
    }

    void DescriptorWriter::buildOrThrow(VkDescriptorSet& set) {
        VkResult outResult = VK_ERROR_INITIALIZATION_FAILED;
        if (!build(set, &outResult)) {
            throw std::runtime_error(std::string("DescriptorWriter::build failed (VkResult=") + std::to_string(outResult) + ")");
        }
    }

    void DescriptorWriter::overwrite(VkDescriptorSet& set) {
        for (auto& write : writes) {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(pool.device.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        writes.clear();
    }
}  // namespace engine
