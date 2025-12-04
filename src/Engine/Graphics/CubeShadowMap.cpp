#include "Engine/Graphics/CubeShadowMap.hpp"

#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace engine {

  CubeShadowMap::CubeShadowMap(Device& device, uint32_t size) : device_{device}, size_{size}
  {
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createSampler();
  }

  CubeShadowMap::~CubeShadowMap()
  {
    vkDestroySampler(device_.device(), sampler_, nullptr);
    for (int i = 0; i < 6; i++)
    {
      vkDestroyFramebuffer(device_.device(), framebuffers_[i], nullptr);
      vkDestroyImageView(device_.device(), faceImageViews_[i], nullptr);
    }
    vkDestroyImageView(device_.device(), cubeImageView_, nullptr);
    vkDestroyRenderPass(device_.device(), renderPass_, nullptr);
    vkDestroyImage(device_.device(), depthImage_, nullptr);
    vkFreeMemory(device_.device(), depthImageMemory_, nullptr);
  }

  void CubeShadowMap::createDepthResources()
  {
    // Create cube map image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = depthFormat_;
    imageInfo.extent.width  = size_;
    imageInfo.extent.height = size_;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6; // 6 faces for cube map
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &depthImage_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cube shadow map image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_.device(), depthImage_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device_.getMemory().findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &depthImageMemory_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate cube shadow map memory");
    }

    vkBindImageMemory(device_.device(), depthImage_, depthImageMemory_, 0);

    // Create cube image view (for sampling in shader)
    VkImageViewCreateInfo cubeViewInfo{};
    cubeViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    cubeViewInfo.image                           = depthImage_;
    cubeViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    cubeViewInfo.format                          = depthFormat_;
    cubeViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    cubeViewInfo.subresourceRange.baseMipLevel   = 0;
    cubeViewInfo.subresourceRange.levelCount     = 1;
    cubeViewInfo.subresourceRange.baseArrayLayer = 0;
    cubeViewInfo.subresourceRange.layerCount     = 6;

    if (vkCreateImageView(device_.device(), &cubeViewInfo, nullptr, &cubeImageView_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cube shadow map image view");
    }

    // Create individual face image views (for rendering)
    for (int i = 0; i < 6; i++)
    {
      VkImageViewCreateInfo faceViewInfo{};
      faceViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      faceViewInfo.image                           = depthImage_;
      faceViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      faceViewInfo.format                          = depthFormat_;
      faceViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      faceViewInfo.subresourceRange.baseMipLevel   = 0;
      faceViewInfo.subresourceRange.levelCount     = 1;
      faceViewInfo.subresourceRange.baseArrayLayer = i;
      faceViewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device_.device(), &faceViewInfo, nullptr, &faceImageViews_[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("Failed to create cube shadow map face image view");
      }
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
    barrier.subresourceRange.layerCount     = 6;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

  void CubeShadowMap::createRenderPass()
  {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat_;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // Since we transition per-layer before and after, use same layout
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Add subpass dependencies for proper synchronization
    std::array<VkSubpassDependency, 2> dependencies;

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
      throw std::runtime_error("Failed to create cube shadow map render pass");
    }
  }

  void CubeShadowMap::createFramebuffers()
  {
    for (int i = 0; i < 6; i++)
    {
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = renderPass_;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments    = &faceImageViews_[i];
      framebufferInfo.width           = size_;
      framebufferInfo.height          = size_;
      framebufferInfo.layers          = 1;

      if (vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("Failed to create cube shadow map framebuffer");
      }
    }
  }

  void CubeShadowMap::createSampler()
  {
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
    samplerInfo.maxLod        = 1.0f;
    samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_FALSE; // We'll compare manually in shader
    samplerInfo.compareOp     = VK_COMPARE_OP_LESS;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cube shadow map sampler");
    }
  }

  glm::mat4 CubeShadowMap::getFaceViewMatrix(const glm::vec3& lightPos, int face)
  {
    // Face directions: +X, -X, +Y, -Y, +Z, -Z
    static const glm::vec3 targets[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),  // +X
            glm::vec3(-1.0f, 0.0f, 0.0f), // -X
            glm::vec3(0.0f, 1.0f, 0.0f),  // +Y
            glm::vec3(0.0f, -1.0f, 0.0f), // -Y
            glm::vec3(0.0f, 0.0f, 1.0f),  // +Z
            glm::vec3(0.0f, 0.0f, -1.0f), // -Z
    };

    static const glm::vec3 ups[6] = {
            glm::vec3(0.0f, -1.0f, 0.0f), // +X
            glm::vec3(0.0f, -1.0f, 0.0f), // -X
            glm::vec3(0.0f, 0.0f, 1.0f),  // +Y
            glm::vec3(0.0f, 0.0f, -1.0f), // -Y
            glm::vec3(0.0f, -1.0f, 0.0f), // +Z
            glm::vec3(0.0f, -1.0f, 0.0f), // -Z
    };

    return glm::lookAt(lightPos, lightPos + targets[face], ups[face]);
  }

  glm::mat4 CubeShadowMap::getProjectionMatrix(float nearPlane, float farPlane)
  {
    // 90 degree FOV for cube faces
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
    // Vulkan Y flip
    proj[1][1] *= -1;
    return proj;
  }

  void CubeShadowMap::beginRenderPass(VkCommandBuffer commandBuffer, int face)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass_;
    renderPassInfo.framebuffer       = framebuffers_[face];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {size_, size_};

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(size_);
    viewport.height   = static_cast<float>(size_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {size_, size_}};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void CubeShadowMap::endRenderPass(VkCommandBuffer commandBuffer)
  {
    vkCmdEndRenderPass(commandBuffer);
  }

  void CubeShadowMap::transitionToAttachmentLayout(VkCommandBuffer commandBuffer)
  {
    // Transition ALL 6 layers from shader read (or undefined) to attachment optimal
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about previous contents
    barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = depthImage_;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6; // All 6 faces

    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
  }

  void CubeShadowMap::transitionToShaderReadLayout(VkCommandBuffer commandBuffer)
  {
    // Transition ALL 6 layers from attachment optimal to shader read
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = depthImage_;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6; // All 6 faces

    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
  }

} // namespace engine
