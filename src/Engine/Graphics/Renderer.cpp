#include "Engine/Graphics/Renderer.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>
#include <unordered_map>

#include "Engine/Core/Exceptions.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

  Renderer::Renderer(Window& window, Device& device) : window{window}, device{device}
  {
    recreateSwapChain();
    createCommandBuffers();
  }

  Renderer::~Renderer()
  {
    freeCommandBuffers();
    freeOffscreenResources();
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
    freeOffscreenResources();
    createOffscreenResources();

    // TODO: recreate other resources dependent on swap chain (e.g.,
    // pipelines) the pipeline may not need to be recreated here if using
    // dynamic viewport/scissor
  }

  VkCommandBuffer Renderer::beginFrame()
  {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");

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

    if (auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized())
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
    VkExtent2D extent      = swapChain->getSwapChainExtent();
    VkFormat   colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat   depthFormat = swapChain->findDepthFormat();

    offscreenMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;

    offscreenColorImages.resize(SwapChain::maxFramesInFlight());
    offscreenColorImageMemorys.resize(SwapChain::maxFramesInFlight());
    offscreenColorImageViews.resize(SwapChain::maxFramesInFlight());
    offscreenColorAttachmentImageViews.resize(SwapChain::maxFramesInFlight());
    offscreenDepthImages.resize(SwapChain::maxFramesInFlight());
    offscreenDepthImageMemorys.resize(SwapChain::maxFramesInFlight());
    offscreenDepthImageViews.resize(SwapChain::maxFramesInFlight());

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      // Create Color Image
      VkImageCreateInfo imageInfo{};
      imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType     = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width  = extent.width;
      imageInfo.extent.height = extent.height;
      imageInfo.extent.depth  = 1;
      imageInfo.mipLevels     = offscreenMipLevels;
      imageInfo.arrayLayers   = 1;
      imageInfo.format        = colorFormat;
      imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.flags       = 0;

      device.getMemory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenColorImages[i], offscreenColorImageMemorys[i]);

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = offscreenColorImages[i];
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = colorFormat;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = offscreenMipLevels;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &offscreenColorImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create offscreen color image view!");
      }

      // Create Color Attachment Image View (Mip Level 0 only)
      VkImageViewCreateInfo attachmentViewInfo       = viewInfo;
      attachmentViewInfo.subresourceRange.levelCount = 1;
      if (vkCreateImageView(device.device(), &attachmentViewInfo, nullptr, &offscreenColorAttachmentImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create offscreen color attachment image view!");
      }

      // Create Depth Image
      VkImageCreateInfo depthImageInfo{};
      depthImageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      depthImageInfo.imageType     = VK_IMAGE_TYPE_2D;
      depthImageInfo.extent.width  = extent.width;
      depthImageInfo.extent.height = extent.height;
      depthImageInfo.extent.depth  = 1;
      depthImageInfo.mipLevels     = 1;
      depthImageInfo.arrayLayers   = 1;
      depthImageInfo.format        = depthFormat;
      depthImageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      depthImageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      depthImageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
      depthImageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
      depthImageInfo.flags         = 0;

      device.getMemory().createImageWithInfo(depthImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenDepthImages[i], offscreenDepthImageMemorys[i]);

      VkImageViewCreateInfo depthViewInfo{};
      depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      depthViewInfo.image                           = offscreenDepthImages[i];
      depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      depthViewInfo.format                          = depthFormat;
      depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      depthViewInfo.subresourceRange.baseMipLevel   = 0;
      depthViewInfo.subresourceRange.levelCount     = 1;
      depthViewInfo.subresourceRange.baseArrayLayer = 0;
      depthViewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &depthViewInfo, nullptr, &offscreenDepthImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create offscreen depth image view!");
      }
    }

    // Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter     = VK_FILTER_LINEAR;
    samplerInfo.minFilter     = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias    = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod        = 0.0f;
    samplerInfo.maxLod        = static_cast<float>(offscreenMipLevels);
    samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &offscreenSampler) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create offscreen sampler!");
    }

    createOffscreenRenderPass();
    createOffscreenFramebuffers();
  }

  void Renderer::createOffscreenRenderPass()
  {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = swapChain->findDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &offscreenRenderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create offscreen render pass!");
    }
  }

  void Renderer::createOffscreenFramebuffers()
  {
    offscreenFramebuffers.resize(SwapChain::maxFramesInFlight());
    VkExtent2D extent = swapChain->getSwapChainExtent();

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      std::array<VkImageView, 2> attachments = {offscreenColorAttachmentImageViews[i], offscreenDepthImageViews[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = offscreenRenderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments    = attachments.data();
      framebufferInfo.width           = extent.width;
      framebufferInfo.height          = extent.height;
      framebufferInfo.layers          = 1;

      if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &offscreenFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create offscreen framebuffer!");
      }
    }
  }

  void Renderer::freeOffscreenResources()
  {
    vkDestroySampler(device.device(), offscreenSampler, nullptr);
    vkDestroyRenderPass(device.device(), offscreenRenderPass, nullptr);

    for (int i = 0; i < offscreenFramebuffers.size(); i++)
    {
      vkDestroyFramebuffer(device.device(), offscreenFramebuffers[i], nullptr);
      vkDestroyImageView(device.device(), offscreenColorImageViews[i], nullptr);
      vkDestroyImageView(device.device(), offscreenColorAttachmentImageViews[i], nullptr);
      vkDestroyImage(device.device(), offscreenColorImages[i], nullptr);
      vkFreeMemory(device.device(), offscreenColorImageMemorys[i], nullptr);

      vkDestroyImageView(device.device(), offscreenDepthImageViews[i], nullptr);
      vkDestroyImage(device.device(), offscreenDepthImages[i], nullptr);
      vkFreeMemory(device.device(), offscreenDepthImageMemorys[i], nullptr);
    }

    offscreenFramebuffers.clear();
    offscreenColorImageViews.clear();
    offscreenColorAttachmentImageViews.clear();
    offscreenColorImages.clear();
    offscreenColorImageMemorys.clear();
    offscreenDepthImageViews.clear();
    offscreenDepthImages.clear();
    offscreenDepthImageMemorys.clear();
  }

  void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    VkClearValue clearValues[2];
    clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = offscreenRenderPass;
    renderPassInfo.framebuffer       = offscreenFramebuffers[currentFrameIndex]; // Use currentFrameIndex for offscreen resources
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->getSwapChainExtent();
    renderPassInfo.clearValueCount   = 2;
    renderPassInfo.pClearValues      = clearValues;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    vkCmdEndRenderPass(commandBuffer);
  }

  VkDescriptorImageInfo Renderer::getOffscreenImageInfo(int index) const
  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = offscreenColorImageViews[index];
    imageInfo.sampler     = offscreenSampler;
    return imageInfo;
  }

  void Renderer::generateOffscreenMipmaps(VkCommandBuffer commandBuffer)
  {
    VkImage    image     = offscreenColorImages[currentFrameIndex];
    VkExtent2D extent    = swapChain->getSwapChainExtent();
    int32_t    mipWidth  = extent.width;
    int32_t    mipHeight = extent.height;

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    // Transition Mip 0 to TRANSFER_SRC
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    for (uint32_t i = 1; i < offscreenMipLevels; i++)
    {
      // Transition Mip i to TRANSFER_DST
      barrier.subresourceRange.baseMipLevel = i;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.srcAccessMask                 = 0;
      barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      VkImageBlit blit{};
      blit.srcOffsets[0]                 = {0, 0, 0};
      blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
      blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel       = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount     = 1;
      blit.dstOffsets[0]                 = {0, 0, 0};
      blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
      blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel       = i;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount     = 1;

      vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

      // Transition Mip i-1 to SHADER_READ_ONLY
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      // Transition Mip i to TRANSFER_SRC (for next loop)
      if (i < offscreenMipLevels - 1)
      {
        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
      }

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition Last Mip to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = offscreenMipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

} // namespace engine
