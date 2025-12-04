#include "Engine/Graphics/ShadowMap.hpp"

#include <array>
#include <stdexcept>

namespace engine {

  ShadowMap::ShadowMap(Device& device, uint32_t width, uint32_t height) : device_{device}, width_{width}, height_{height}
  {
    createDepthResources();
    createRenderPass();
    createFramebuffer();
    createSampler();
  }

  ShadowMap::~ShadowMap()
  {
    if (sampler_ != VK_NULL_HANDLE)
    {
      vkDestroySampler(device_.device(), sampler_, nullptr);
    }
    if (framebuffer_ != VK_NULL_HANDLE)
    {
      vkDestroyFramebuffer(device_.device(), framebuffer_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE)
    {
      vkDestroyRenderPass(device_.device(), renderPass_, nullptr);
    }
    if (depthImageView_ != VK_NULL_HANDLE)
    {
      vkDestroyImageView(device_.device(), depthImageView_, nullptr);
    }
    if (depthImage_ != VK_NULL_HANDLE)
    {
      vkDestroyImage(device_.device(), depthImage_, nullptr);
    }
    if (depthImageMemory_ != VK_NULL_HANDLE)
    {
      vkFreeMemory(device_.device(), depthImageMemory_, nullptr);
    }
  }

  void ShadowMap::createDepthResources()
  {
    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width_;
    imageInfo.extent.height = height_;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = depthFormat_;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &depthImage_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow map depth image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_.device(), depthImage_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device_.memory().findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &depthImageMemory_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate shadow map depth image memory");
    }

    vkBindImageMemory(device_.device(), depthImage_, depthImageMemory_, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = depthImage_;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = depthFormat_;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow map depth image view");
    }

    // Transition to READ_ONLY_OPTIMAL initially so it's valid for binding
    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = depthImage_;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

  void ShadowMap::createRenderPass()
  {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat_;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0;
    subpass.pColorAttachments       = nullptr;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Dependency to ensure depth writes complete before sampling
    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &depthAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow map render pass");
    }
  }

  void ShadowMap::createFramebuffer()
  {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = renderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments    = &depthImageView_;
    framebufferInfo.width           = width_;
    framebufferInfo.height          = height_;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow map framebuffer");
    }
  }

  void ShadowMap::createSampler()
  {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1.0f;
    samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // Outside shadow map = no shadow
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_TRUE;
    samplerInfo.compareOp               = VK_COMPARE_OP_LESS_OR_EQUAL; // PCF comparison
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 1.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow map sampler");
    }
  }

  void ShadowMap::beginRenderPass(VkCommandBuffer commandBuffer)
  {
    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass_;
    renderPassInfo.framebuffer       = framebuffer_;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {width_, height_};
    renderPassInfo.clearValueCount   = 1;
    renderPassInfo.pClearValues      = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(width_);
    viewport.height   = static_cast<float>(height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width_, height_};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void ShadowMap::endRenderPass(VkCommandBuffer commandBuffer)
  {
    vkCmdEndRenderPass(commandBuffer);
  }

} // namespace engine
