#include "Engine/Graphics/Renderer.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "vulkan/vulkan_core.h"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

  Renderer::Renderer(Window& window, Device& device) : window{window}, device{device}, hzbGenerator{device}
  {
    recreateSwapChain();
    createCommandBuffers();
  }

  Renderer::~Renderer()
  {
    freeCommandBuffers();
  }

  void Renderer::createCommandBuffers()
  {
    commandBuffers.resize(SwapChain::maxFramesInFlight());

    if (VkCommandBufferAllocateInfo const allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = device.getCommandPool(),
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        };
        vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to allocate command buffers!");
    }
  }

  void Renderer::freeCommandBuffers()
  {
    vkFreeCommandBuffers(device.device(), device.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
  }

  void Renderer::recreateSwapChain()
  {
    VkExtent2D extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0)
    {
      extent = window.getExtent();
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(device.device());

    if (swapChain == nullptr)
    {
      swapChain = std::make_unique<SwapChain>(device, extent);
    }
    else
    {
      std::shared_ptr<SwapChain> const oldSwapChain = std::move(swapChain);
      swapChain                               = std::make_unique<SwapChain>(device, extent, oldSwapChain);

      if (!oldSwapChain->compareSwapFormats(*swapChain))
      {
        throw SwapChainCreationException("Swap chain image or depth format has changed!");
      }
    }

    // Recreate offscreen resources to match new swapchain extent
    if (offscreenFrameBuffer)
    {
      offscreenFrameBuffer->resize(swapChain->getSwapChainExtent());
    }
    else
    {
      createOffscreenResources();
    }

    createHZBPipeline();

    // Initialize offscreen image layouts after recreation.
    // Depth prepass expects the depth image in DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    // and the HZB pass transitions it to/from SHADER_READ_ONLY_OPTIMAL internally.
    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      VkImageMemoryBarrier barrier{};
      barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                           = offscreenFrameBuffer->getDepthImage(i);
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      barrier.subresourceRange.baseMipLevel   = 0;
      barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = 1;
      barrier.srcAccessMask                   = 0;
      barrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &barrier);

      VkImageMemoryBarrier hzbBarrier{};
      hzbBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      hzbBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      hzbBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      hzbBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      hzbBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      hzbBarrier.image                           = offscreenFrameBuffer->getHzbImage(i);
      hzbBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      hzbBarrier.subresourceRange.baseMipLevel   = 0;
      hzbBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      hzbBarrier.subresourceRange.baseArrayLayer = 0;
      hzbBarrier.subresourceRange.layerCount     = 1;
      hzbBarrier.srcAccessMask                   = 0;
      hzbBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &hzbBarrier);

      // Initialize scene-color copy image layout for safe sampling/copying.
      VkImageMemoryBarrier sceneBarrier{};
      sceneBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      sceneBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      sceneBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      sceneBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      sceneBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      sceneBarrier.image                           = offscreenFrameBuffer->getSceneColorImage(i);
      sceneBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      sceneBarrier.subresourceRange.baseMipLevel   = 0;
      sceneBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      sceneBarrier.subresourceRange.baseArrayLayer = 0;
      sceneBarrier.subresourceRange.layerCount     = 1;
      sceneBarrier.srcAccessMask                   = 0;
      sceneBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &sceneBarrier);
    }

    device.endSingleTimeCommands(commandBuffer);

    // TODO: recreate other resources dependent on swap chain (e.g.,
    // pipelines) the pipeline may not need to be recreated here if using
    // dynamic viewport/scissor
    swapChainRecreated = true;
  }

  VkCommandBuffer Renderer::beginFrame()
  {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");
    swapChainRecreated = false;

    uint32_t imageIndex;
    auto     result = swapChain->acquireNextImage(&imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
      recreateSwapChain();
      return nullptr;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
      throw SwapChainCreationException("failed to acquire swap chain image!");
    }

    currentImageIndex             = imageIndex;
    VkCommandBuffer commandBuffer = commandBuffers[currentFrameIndex];
    if (vkResetCommandBuffer(commandBuffer, /*flags=*/0) != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to reset command buffer!");
    }
    isFrameStarted = true;
    VkCommandBufferBeginInfo const beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    if (auto beginCommandBufferResult = vkBeginCommandBuffer(commandBuffer, &beginInfo); beginCommandBufferResult != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to begin recording command buffer!");
    }

    return commandBuffer;
  }

  void Renderer::endFrame()
  {
    assert(isFrameStarted && "Can't call endFrame while frame not in progress");

    auto commandBuffer = getCurrentCommandBuffer();

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to record command buffer!");
    }

    if (auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex); result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized())
    {
      window.resetWindowResizedFlag();
      recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
      throw SwapChainCreationException("failed to present swap chain image!");
    }

    isFrameStarted    = false;
    currentFrameIndex = (currentFrameIndex + 1) % SwapChain::maxFramesInFlight();
  }

  void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different "
                                                         "frame");

    VkClearValue clearValues[] = {
            {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
            {.depthStencil = {1.0f, 0}},
    };

    VkRenderPassBeginInfo const renderPassInfo{
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = swapChain->getRenderPass(),
            .framebuffer = swapChain->getFrameBuffer(currentImageIndex),
            .renderArea =
                    {
                            .offset = {0, 0},
                            .extent = swapChain->getSwapChainExtent(),
                    },
            .clearValueCount = static_cast<uint32_t>(std::size(clearValues)),
            .pClearValues    = clearValues,
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport const viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(swapChain->getSwapChainExtent().width),
            .height   = static_cast<float>(swapChain->getSwapChainExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D const scissor{
            .offset = {0, 0},
            .extent = swapChain->getSwapChainExtent(),
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't end render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on a command buffer from a different "
                                                         "frame");
    vkCmdEndRenderPass(commandBuffer);
  }

  void Renderer::createOffscreenResources()
  {
    offscreenFrameBuffer = std::make_unique<FrameBuffer>(device, swapChain->getSwapChainExtent(), SwapChain::maxFramesInFlight(), true);
  }

  void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginRenderPass(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::beginOffscreenDepthPrepassRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginDepthPrepassRenderPass(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::beginOffscreenRenderPassLoadDepth(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginRenderPassLoadDepth(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::beginOffscreenRenderPassLoadColorDepth(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginRenderPassLoadColorDepth(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::beginGbufferRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginGbufferRenderPass(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginDeferredLightingRenderPass(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::endOffscreenRenderPass(VkCommandBuffer commandBuffer) const
  {
    assert(isFrameStarted && "Can't end render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on a command buffer from a different frame");
    offscreenFrameBuffer->endRenderPass(commandBuffer);
  }

  VkDescriptorImageInfo Renderer::getOffscreenImageInfo(int index) const
  {
    return offscreenFrameBuffer->getDescriptorImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getSceneColorImageInfo(int index) const
  {
    return offscreenFrameBuffer->getSceneColorDescriptorImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getGbufferNormalImageInfo(int index) const
  {
    return offscreenFrameBuffer->getGbufferNormalImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getGbufferAlbedoImageInfo(int index) const
  {
    return offscreenFrameBuffer->getGbufferAlbedoImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getGbufferMaterialImageInfo(int index) const
  {
    return offscreenFrameBuffer->getGbufferMaterialImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getDepthImageInfo(int index) const
  {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = offscreenFrameBuffer->getDepthImageView(index);
    info.sampler     = offscreenFrameBuffer->getDepthSampler();
    return info;
  }

  VkDescriptorImageInfo Renderer::getHzbImageInfo(int index) const
  {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = offscreenFrameBuffer->getHzbImageView(index);
    info.sampler     = offscreenFrameBuffer->getHzbSampler();
    return info;
  }

  void Renderer::copyOffscreenColorToSceneColor(VkCommandBuffer commandBuffer)
  {
    if (!offscreenFrameBuffer) return;

    VkImage srcImage = offscreenFrameBuffer->getColorImage(currentFrameIndex);
    VkImage dstImage = offscreenFrameBuffer->getSceneColorImage(currentFrameIndex);

    // 1) Transition src mip0: COLOR_ATTACHMENT -> TRANSFER_SRC
    VkImageMemoryBarrier srcBarrier{};
    srcBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    srcBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    srcBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.image                           = srcImage;
    srcBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    srcBarrier.subresourceRange.baseMipLevel   = 0;
    srcBarrier.subresourceRange.levelCount     = 1;
    srcBarrier.subresourceRange.baseArrayLayer = 0;
    srcBarrier.subresourceRange.layerCount     = 1;
    srcBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;

    // 2) Transition dst: SHADER_READ -> TRANSFER_DST
    VkImageMemoryBarrier dstBarrier{};
    dstBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dstBarrier.oldLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dstBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    dstBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    dstBarrier.image                           = dstImage;
    dstBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    dstBarrier.subresourceRange.baseMipLevel   = 0;
    dstBarrier.subresourceRange.levelCount     = 1;
    dstBarrier.subresourceRange.baseArrayLayer = 0;
    dstBarrier.subresourceRange.layerCount     = 1;
    dstBarrier.srcAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
    dstBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

    std::array<VkImageMemoryBarrier, 2> barriers = {srcBarrier, dstBarrier};
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         static_cast<uint32_t>(barriers.size()),
                         barriers.data());

    VkImageCopy region{};
    region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel       = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel       = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.extent.width                  = swapChain->getSwapChainExtent().width;
    region.extent.height                 = swapChain->getSwapChainExtent().height;
    region.extent.depth                  = 1;

    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // 3) Transition src back to COLOR_ATTACHMENT for subsequent passes + mipmap generation
    VkImageMemoryBarrier srcToColor{};
    srcToColor.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    srcToColor.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcToColor.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    srcToColor.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    srcToColor.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    srcToColor.image                           = srcImage;
    srcToColor.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    srcToColor.subresourceRange.baseMipLevel   = 0;
    srcToColor.subresourceRange.levelCount     = 1;
    srcToColor.subresourceRange.baseArrayLayer = 0;
    srcToColor.subresourceRange.layerCount     = 1;
    srcToColor.srcAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
    srcToColor.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &srcToColor);

    // 4) Build sceneColor mip chain for rough transmission blur (finishes in SHADER_READ_ONLY)
    if (offscreenFrameBuffer->getMipLevels() > 1)
    {
      offscreenFrameBuffer->generateSceneColorMipmaps(commandBuffer, currentFrameIndex);
    }
    else
    {
      // No mip chain: just transition mip0 back to SHADER_READ.
      VkImageMemoryBarrier dstToRead{};
      dstToRead.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      dstToRead.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      dstToRead.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      dstToRead.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      dstToRead.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      dstToRead.image                           = dstImage;
      dstToRead.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      dstToRead.subresourceRange.baseMipLevel   = 0;
      dstToRead.subresourceRange.levelCount     = 1;
      dstToRead.subresourceRange.baseArrayLayer = 0;
      dstToRead.subresourceRange.layerCount     = 1;
      dstToRead.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
      dstToRead.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &dstToRead);
    }
  }

  void Renderer::generateOffscreenMipmaps(VkCommandBuffer commandBuffer)
  {
    offscreenFrameBuffer->generateMipmaps(commandBuffer, currentFrameIndex);
  }

  void Renderer::createHZBPipeline()
  {
    if (!offscreenFrameBuffer) return;
    hzbGenerator.recreateDescriptors(*offscreenFrameBuffer, swapChain->getSwapChainExtent(), static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
  }

  void Renderer::generateDepthPyramid(VkCommandBuffer commandBuffer)
  {
    if (!offscreenFrameBuffer) return;
    hzbGenerator.generate(commandBuffer, *offscreenFrameBuffer, swapChain->getSwapChainExtent(), currentFrameIndex);
  }

} // namespace engine
