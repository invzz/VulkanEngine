#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

// =============================================================================
// Buffer Creation Tests
// =============================================================================

TEST(Buffer, GivenValidDevice_WhenBufferCreated_ThenHandleIsValid)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize instanceSize  = 64;
  constexpr uint32_t     instanceCount = 10;

  Buffer buffer(device, instanceSize, instanceCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  EXPECT_NE(buffer.getBuffer(), VK_NULL_HANDLE);
}

TEST(Buffer, GivenBufferWithAlignment_WhenCreated_ThenAlignmentCalculatedCorrectly)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize instanceSize       = 65; // Not aligned
  constexpr uint32_t     instanceCount      = 4;
  constexpr VkDeviceSize minOffsetAlignment = 64; // Common UBO alignment

  Buffer buffer(device, instanceSize, instanceCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, minOffsetAlignment);

  // Alignment should round up 65 to 128 (next multiple of 64)
  EXPECT_EQ(buffer.getAlignmentSize(), 128);
  EXPECT_EQ(buffer.getBufferSize(), 128 * instanceCount);
}

// =============================================================================
// Buffer Mapping Tests
// =============================================================================

TEST(Buffer, GivenHostVisibleBuffer_WhenMapped_ThenReturnsSuccess)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize bufferSize = 256;

  Buffer buffer(device, bufferSize, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkResult result = buffer.map();
  EXPECT_EQ(result, VK_SUCCESS);

  buffer.unmap();
}

TEST(Buffer, GivenMappedBuffer_WhenUnmapped_ThenMappedPointerIsNull)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  Buffer buffer(device, 256, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();
  EXPECT_NE(buffer.getMappedMemory(), nullptr);

  buffer.unmap();
  EXPECT_EQ(buffer.getMappedMemory(), nullptr);
}

TEST(Buffer, GivenUnmappedBuffer_WhenUnmapCalledAgain_ThenNoError)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  Buffer buffer(device, 256, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Should not crash calling unmap without map
  buffer.unmap();
  buffer.unmap(); // Double unmap should be safe

  SUCCEED();
}

// =============================================================================
// Buffer Write Tests
// =============================================================================

TEST(Buffer, GivenMappedBuffer_WhenDataWritten_ThenDataIsStored)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize bufferSize = 256;

  Buffer buffer(device, bufferSize, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  // Write test data
  std::vector<uint8_t> testData(bufferSize, 0xAB);
  buffer.writeToBuffer(testData.data(), bufferSize);

  // Verify data was written
  auto* mapped = static_cast<uint8_t*>(buffer.getMappedMemory());
  for (size_t i = 0; i < bufferSize; ++i)
  {
    EXPECT_EQ(mapped[i], 0xAB) << "Mismatch at byte " << i;
  }

  buffer.unmap();
}

TEST(Buffer, GivenMappedBuffer_WhenWrittenWithOffset_ThenCorrectBytesAffected)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize bufferSize = 256;

  Buffer buffer(device, bufferSize, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  // Zero out buffer first
  std::vector<uint8_t> zeros(bufferSize, 0);
  buffer.writeToBuffer(zeros.data(), bufferSize);

  // Write data at offset
  constexpr VkDeviceSize offset   = 64;
  constexpr VkDeviceSize dataSize = 32;
  std::vector<uint8_t>   testData(dataSize, 0xFF);
  buffer.writeToBuffer(testData.data(), dataSize, offset);

  // Verify only the right bytes were affected
  auto* mapped = static_cast<uint8_t*>(buffer.getMappedMemory());

  for (size_t i = 0; i < offset; ++i)
  {
    EXPECT_EQ(mapped[i], 0) << "Byte before offset should be 0, at " << i;
  }

  for (size_t i = offset; i < offset + dataSize; ++i)
  {
    EXPECT_EQ(mapped[i], 0xFF) << "Written byte should be 0xFF, at " << i;
  }

  for (size_t i = offset + dataSize; i < bufferSize; ++i)
  {
    EXPECT_EQ(mapped[i], 0) << "Byte after data should be 0, at " << i;
  }

  buffer.unmap();
}

// =============================================================================
// Buffer Index Operations Tests
// =============================================================================

TEST(Buffer, GivenMultiInstanceBuffer_WhenWriteToIndex_ThenCorrectInstanceWritten)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize instanceSize  = 64;
  constexpr uint32_t     instanceCount = 4;

  Buffer buffer(device, instanceSize, instanceCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  // Write different values to each index
  for (uint32_t i = 0; i < instanceCount; ++i)
  {
    std::vector<uint8_t> data(instanceSize, static_cast<uint8_t>(i + 1));
    buffer.writeToIndex(data.data(), i);
  }

  // Verify each instance has correct data
  auto* mapped = static_cast<uint8_t*>(buffer.getMappedMemory());
  for (uint32_t i = 0; i < instanceCount; ++i)
  {
    uint8_t expected  = static_cast<uint8_t>(i + 1);
    size_t  byteIndex = i * instanceSize;
    EXPECT_EQ(mapped[byteIndex], expected) << "Instance " << i << " has wrong data";
  }

  buffer.unmap();
}

// =============================================================================
// Buffer Descriptor Info Tests
// =============================================================================

TEST(Buffer, GivenBuffer_WhenDescriptorInfoRequested_ThenCorrectValuesReturned)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize instanceSize  = 64;
  constexpr uint32_t     instanceCount = 4;

  Buffer buffer(device, instanceSize, instanceCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkDescriptorBufferInfo info = buffer.descriptorInfo(128, 64);

  EXPECT_EQ(info.buffer, buffer.getBuffer());
  EXPECT_EQ(info.offset, 64);
  EXPECT_EQ(info.range, 128);
}

TEST(Buffer, GivenMultiInstanceBuffer_WhenDescriptorInfoForIndex_ThenOffsetsCorrect)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  constexpr VkDeviceSize instanceSize  = 64;
  constexpr uint32_t     instanceCount = 4;

  Buffer buffer(device, instanceSize, instanceCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  for (int i = 0; i < static_cast<int>(instanceCount); ++i)
  {
    VkDescriptorBufferInfo info = buffer.descriptorInfoForIndex(i);
    EXPECT_EQ(info.offset, static_cast<VkDeviceSize>(i) * instanceSize);
    EXPECT_EQ(info.range, instanceSize);
  }
}

// =============================================================================
// Buffer Flush Tests
// =============================================================================

TEST(Buffer, GivenMappedNonCoherentBuffer_WhenFlushed_ThenReturnsSuccess)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  // Note: We use host coherent for simplicity, but flush should still work
  Buffer buffer(device, 256, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  VkResult result = buffer.flush();
  EXPECT_EQ(result, VK_SUCCESS);

  buffer.unmap();
}

TEST(Buffer, GivenMultiInstanceBuffer_WhenFlushIndex_ThenReturnsSuccess)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  Buffer buffer(device, 64, 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  for (int i = 0; i < 4; ++i)
  {
    VkResult result = buffer.flushIndex(i);
    EXPECT_EQ(result, VK_SUCCESS) << "Flush failed for index " << i;
  }

  buffer.unmap();
}

// =============================================================================
// Buffer Device Address Tests
// =============================================================================

TEST(Buffer, GivenBufferWithDeviceAddress_WhenAddressRequested_ThenNonZero)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  // Create buffer with device address usage flag
  Buffer buffer(device, 256, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkDeviceAddress address = buffer.getDeviceAddress();

  // Device address should be non-zero for valid buffer
  EXPECT_NE(address, 0);
}

// =============================================================================
// Buffer Invalidate Tests
// =============================================================================

TEST(Buffer, GivenMappedBuffer_WhenInvalidated_ThenReturnsSuccess)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  Buffer buffer(device, 256, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  VkResult result = buffer.invalidate();
  EXPECT_EQ(result, VK_SUCCESS);

  buffer.unmap();
}

TEST(Buffer, GivenMultiInstanceBuffer_WhenInvalidateIndex_ThenReturnsSuccess)
{
  Window window(1, 1, "BufferTest");
  Device device(window);

  Buffer buffer(device, 64, 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  buffer.map();

  for (int i = 0; i < 4; ++i)
  {
    VkResult result = buffer.invalidateIndex(i);
    EXPECT_EQ(result, VK_SUCCESS) << "Invalidate failed for index " << i;
  }

  buffer.unmap();
}
