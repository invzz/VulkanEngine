#pragma once

#include "Device.hpp"

namespace engine {
  class Buffer
  {
  public:
    Buffer(Device&               device,
           VkDeviceSize          instanceSize,
           uint32_t              instanceCount,
           VkBufferUsageFlags    usageFlags,
           VkMemoryPropertyFlags memoryPropertyFlags,
           VkDeviceSize          minOffsetAlignment = 1);
    ~Buffer();
    Buffer(const Buffer&)            = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void     unmap();

    void                   writeToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult               flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult               invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void                   writeToIndex(void* data, int index);
    VkResult               flushIndex(int index);
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);
    VkResult               invalidateIndex(int index);

    // getters
    VkBuffer              getBuffer() const { return buffer; }
    void*                 getMappedMemory() const { return mapped; }
    uint32_t              getInstanceCount() const { return instanceCount; }
    VkDeviceSize          getInstanceSize() const { return instanceSize; }
    VkDeviceSize          getAlignmentSize() const { return alignmentSize; }
    VkDeviceSize          getBufferSize() const { return bufferSize; }
    VkBufferUsageFlags    getUsageFlags() const { return usageFlags; }
    VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }

  private:
    static VkDeviceSize   getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
    Device&               device;
    void*                 mapped        = nullptr;
    VkBuffer              buffer        = VK_NULL_HANDLE;
    VkDeviceMemory        memory        = VK_NULL_HANDLE;
    VkDeviceSize          bufferSize    = 0;
    VkDeviceSize          instanceSize  = 0;
    uint32_t              instanceCount = 0;
    VkDeviceSize          alignmentSize = 0;
    VkBufferUsageFlags    usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;
  };
} // namespace engine
