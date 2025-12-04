#include "Engine/Graphics/FrameBuffer.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace engine {

  FrameBuffer::FrameBuffer(Device& device, VkExtent2D extent, uint32_t frameCount, bool useMipmaps)
      : device{device}, extent{extent}, frameCount{frameCount}, useMipmaps{useMipmaps}
  {
    createRenderPass();
    createImages();
    createFramebuffers();
  }

  FrameBuffer::~FrameBuffer()
  {
    cleanup();
    vkDestroyRenderPass(device.device(), renderPass, nullptr);
  }

  void FrameBuffer::cleanup()
  {
    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto imageView : colorImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto imageView : colorAttachmentImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : colorImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : colorImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : depthImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : depthImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : depthImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    if (sampler != VK_NULL_HANDLE)
    {
      vkDestroySampler(device.device(), sampler, nullptr);
      sampler = VK_NULL_HANDLE;
    }
  }

  void FrameBuffer::resize(VkExtent2D newExtent)
  {
    extent = newExtent;
    cleanup();
    createImages();
    createFramebuffers();
  }

  void FrameBuffer::createRenderPass()
  {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

    if (useMipmaps)
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                        VK_IMAGE_TILING_OPTIMAL,
                                                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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

    if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create frame buffer render pass!");
    }
  }

  void FrameBuffer::createImages()
  {
    if (useMipmaps)
    {
      mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    }
    else
    {
      mipLevels = 1;
    }

    colorImages.resize(frameCount);
    colorImageMemorys.resize(frameCount);
    colorImageViews.resize(frameCount);
    colorAttachmentImageViews.resize(frameCount);
    depthImages.resize(frameCount);
    depthImageMemorys.resize(frameCount);
    depthImageViews.resize(frameCount);

    VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depthFormat = device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                      VK_IMAGE_TILING_OPTIMAL,
                                                      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    for (uint32_t i = 0; i < frameCount; i++)
    {
      // Create Color Image
      VkImageCreateInfo imageInfo{};
      imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType     = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width  = extent.width;
      imageInfo.extent.height = extent.height;
      imageInfo.extent.depth  = 1;
      imageInfo.mipLevels     = mipLevels;
      imageInfo.arrayLayers   = 1;
      imageInfo.format        = colorFormat;
      imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

      if (useMipmaps)
      {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      }

      imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

      device.getMemory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImages[i], colorImageMemorys[i]);

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = colorImages[i];
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = colorFormat;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = mipLevels;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &colorImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create color image view!");
      }

      // Create Color Attachment Image View (Mip Level 0 only)
      VkImageViewCreateInfo attachmentViewInfo       = viewInfo;
      attachmentViewInfo.subresourceRange.levelCount = 1;
      if (vkCreateImageView(device.device(), &attachmentViewInfo, nullptr, &colorAttachmentImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create color attachment image view!");
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

      device.getMemory().createImageWithInfo(depthImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImageMemorys[i]);

      VkImageViewCreateInfo depthViewInfo{};
      depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      depthViewInfo.image                           = depthImages[i];
      depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      depthViewInfo.format                          = depthFormat;
      depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      depthViewInfo.subresourceRange.baseMipLevel   = 0;
      depthViewInfo.subresourceRange.levelCount     = 1;
      depthViewInfo.subresourceRange.baseArrayLayer = 0;
      depthViewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &depthViewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create depth image view!");
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
    samplerInfo.maxLod        = static_cast<float>(mipLevels);
    samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create texture sampler!");
    }
  }

  void FrameBuffer::createFramebuffers()
  {
    framebuffers.resize(frameCount);

    for (size_t i = 0; i < frameCount; i++)
    {
      std::array<VkImageView, 2> attachments = {colorAttachmentImageViews[i], depthImageViews[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = renderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments    = attachments.data();
      framebufferInfo.width           = extent.width;
      framebufferInfo.height          = extent.height;
      framebufferInfo.layers          = 1;

      if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create framebuffer!");
      }
    }
  }

  VkDescriptorImageInfo FrameBuffer::getDescriptorImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = colorImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  void FrameBuffer::beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass;
    renderPassInfo.framebuffer       = framebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::endRenderPass(VkCommandBuffer commandBuffer) const
  {
    vkCmdEndRenderPass(commandBuffer);
  }

  void FrameBuffer::generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex)
  {
    if (!useMipmaps) return;

    VkImage image     = colorImages[frameIndex];
    int32_t mipWidth  = extent.width;
    int32_t mipHeight = extent.height;

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

    for (uint32_t i = 1; i < mipLevels; i++)
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
      if (i < mipLevels - 1)
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
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

} // namespace engine
