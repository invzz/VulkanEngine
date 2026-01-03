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

    if (VkCommandBufferAllocateInfo allocInfo{
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
      std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
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

    // Transition all depth images to SHADER_READ_ONLY_OPTIMAL to avoid validation
    // errors on first use
    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      VkImageMemoryBarrier barrier{};
      barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                           = offscreenFrameBuffer->getDepthImage(i);
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      barrier.subresourceRange.baseMipLevel   = 0;
      barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = 1;
      barrier.srcAccessMask                   = 0;
      barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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
    VkCommandBufferBeginInfo beginInfo{
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

    VkRenderPassBeginInfo renderPassInfo{
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

    VkViewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(swapChain->getSwapChainExtent().width),
            .height   = static_cast<float>(swapChain->getSwapChainExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{
            .offset = {0, 0},
            .extent = swapChain->getSwapChainExtent(),
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) const
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

  VkDescriptorImageInfo Renderer::getDepthImageInfo(int index) const
  {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = offscreenFrameBuffer->getDepthImageView(index);
    info.sampler     = offscreenFrameBuffer->getDepthSampler();
    return info;
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
