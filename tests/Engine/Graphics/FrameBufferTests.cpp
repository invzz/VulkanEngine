#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
using namespace engine;
TEST(FrameBuffer, GivenOffscreenImage_WhenMipmapsGenerated_ThenHigherMipsArePopulated) {
    Window                  win(64, 64, "FrameBufferTest");
    Device                  device(win);
    Renderer                renderer(win, device);
    VkCommandBuffer         cmd       = device.beginSingleTimeCommands();
    VkImage                 offscreen = renderer.getOffscreenColorImage(0);
    VkImageSubresourceRange range{};
    range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel   = 0;
    range.levelCount     = 1;
    range.baseArrayLayer = 0;
    range.layerCount     = 1;
    VkClearColorValue clearValue;
    clearValue.float32[0] = 0.5f;
    clearValue.float32[1] = 0.25f;
    clearValue.float32[2] = 0.125f;
    clearValue.float32[3] = 1.0f;
    VkImageMemoryBarrier toDst{};
    toDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toDst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image               = offscreen;
    toDst.subresourceRange    = range;
    toDst.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toDst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
    vkCmdClearColorImage(cmd, offscreen, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
    VkImageMemoryBarrier toColor{};
    toColor.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toColor.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image               = offscreen;
    toColor.subresourceRange    = range;
    toColor.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    toColor.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &toColor);
    renderer.generateOffscreenMipmaps(cmd);
    device.endSingleTimeCommands(cmd);
    device.WaitIdle();
    VkExtent2D winExtent = win.getExtent();
    uint32_t   maxDim    = std::max(winExtent.width, winExtent.height);
    uint32_t   mipLevels = 1;
    while ((1u << mipLevels) <= maxDim)
        mipLevels++;
    ASSERT_GT(mipLevels, 1u);
    const uint32_t mipIndex = 1u;
    const uint32_t readW    = std::max(1u, winExtent.width >> mipIndex);
    const uint32_t readH    = std::max(1u, winExtent.height >> mipIndex);
    VkBuffer       hostBuf;
    VkDeviceMemory hostMem;
    device.getMemory().createBuffer(static_cast<VkDeviceSize>(readW * readH * 4 * sizeof(float)),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        hostBuf,
        hostMem);
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = mipIndex;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {readW, readH, 1};
    device.getMemory().copyImageToBuffer(offscreen, hostBuf, {region}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void* data = nullptr;
    vkMapMemory(device.device(), hostMem, 0, VK_WHOLE_SIZE, 0, &data);
    float* f          = reinterpret_cast<float*>(data);
    bool   anyNonZero = false;
    for (uint32_t i = 0; i < readW * readH * 4; ++i) {
        if (f[i] != 0.0f) {
            anyNonZero = true;
            break;
        }
    }
    vkUnmapMemory(device.device(), hostMem);
    vkDestroyBuffer(device.device(), hostBuf, nullptr);
    vkFreeMemory(device.device(), hostMem, nullptr);
    EXPECT_TRUE(anyNonZero);
}
