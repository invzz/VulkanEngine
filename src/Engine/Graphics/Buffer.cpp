/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https:
 */
#include "Engine/Graphics/Buffer.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Engine/Core/ErrorCodes.hpp"
#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    namespace {
        bool shouldInjectBufferAllocationFailure() {
            const char* env = std::getenv("ENGINE_INJECT_ALLOC_FAILURE");
            if (env == nullptr) {
                return false;
            }
            std::string token(env);
            return token.find("all") != std::string::npos || token.find("buffer") != std::string::npos;
        }
    }  // namespace
    VkDeviceSize Buffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }
    Buffer::Buffer(Device& device, VkDeviceSize instanceSize, uint32_t instanceCount, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment)
        : device{device}, instanceSize{instanceSize}, instanceCount{instanceCount}, usageFlags{usageFlags}, memoryPropertyFlags{memoryPropertyFlags} {
        alignmentSize            = getAlignment(instanceSize, minOffsetAlignment);
        bufferSize               = alignmentSize * instanceCount;
        bool const injectFailure = shouldInjectBufferAllocationFailure();
        try {
            if (injectFailure) {
                throw RuntimeException("Injected buffer allocation failure");
            }
            device.memory().createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
        } catch (const std::exception& primaryError) {
            bool const canFallback = (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u;
            if (!canFallback) {
                ErrorState::report(ErrorCode::BufferAllocationFailure, ErrorBoundary::Fatal, std::string("Buffer allocation failed without fallback: ") + primaryError.what());
                Logger::error(LogChannel::Resource, "Buffer allocation failed (fatal): ", primaryError.what());
                throw;
            }
            VkMemoryPropertyFlags const fallbackFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            try {
                device.memory().createBuffer(bufferSize, usageFlags, fallbackFlags, buffer, memory);
                this->memoryPropertyFlags = fallbackFlags;
                ErrorState::report(
                    ErrorCode::BufferAllocationFallback,
                    ErrorBoundary::Recoverable,
                    "Buffer allocation fell back from DEVICE_LOCAL to HOST_VISIBLE|HOST_COHERENT");
                Logger::warn(LogChannel::Resource, "Buffer allocation fallback activated for size=", bufferSize, " bytes");
            } catch (const std::exception& fallbackError) {
                ErrorState::report(ErrorCode::BufferAllocationFailure, ErrorBoundary::Fatal, std::string("Buffer allocation fallback failed: ") + fallbackError.what());
                Logger::error(LogChannel::Resource, "Buffer allocation fallback failed (fatal): ", fallbackError.what());
                throw;
            }
        }
    }
    Buffer::~Buffer() {
        unmap();
        VkBuffer       buf = buffer;
        VkDeviceMemory mem = memory;
        device.deferDestroy([buf, mem](VkDevice dev) {
            if (buf != VK_NULL_HANDLE) {
                vkDestroyBuffer(dev, buf, nullptr);
            }
            if (mem != VK_NULL_HANDLE) {
                vkFreeMemory(dev, mem, nullptr);
            }
        });
    }
    VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(buffer && memory && "Called map on buffer before create");
        return vkMapMemory(device.device(), memory, offset, size, 0, &mapped);
    }
    void Buffer::unmap() {
        if (mapped != nullptr) {
            vkUnmapMemory(device.device(), memory);
            mapped = nullptr;
        }
    }
    void Buffer::writeToBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");
        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            auto memOffset = (char*) mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }
    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory              = memory;
        mappedRange.offset              = offset;
        mappedRange.size                = size;
        return vkFlushMappedMemoryRanges(device.device(), 1, &mappedRange);
    }
    VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory              = memory;
        mappedRange.offset              = offset;
        mappedRange.size                = size;
        return vkInvalidateMappedMemoryRanges(device.device(), 1, &mappedRange);
    }
    VkDescriptorBufferInfo Buffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
        return VkDescriptorBufferInfo{
            buffer,
            offset,
            size,
        };
    }
    void Buffer::writeToIndex(void* data, int index) {
        writeToBuffer(data, instanceSize, index * alignmentSize);
    }
    VkResult Buffer::flushIndex(int index) {
        return flush(alignmentSize, index * alignmentSize);
    }
    VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(int index) {
        return descriptorInfo(alignmentSize, index * alignmentSize);
    }
    VkResult Buffer::invalidateIndex(int index) {
        return invalidate(alignmentSize, index * alignmentSize);
    }
    VkDeviceAddress Buffer::getDeviceAddress() const {
        VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
        bufferDeviceAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAddressInfo.buffer = buffer;
        return vkGetBufferDeviceAddress(device.device(), &bufferDeviceAddressInfo);
    }
}  // namespace engine