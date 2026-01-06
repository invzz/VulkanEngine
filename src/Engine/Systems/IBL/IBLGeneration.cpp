#include <algorithm>
#include <stdexcept>
#include <vector>

#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"


namespace engine {

  void IBLSystem::ensureBRDFLUT()
  {
    // If we already have a non-fallback LUT at the requested size, keep it.
    // We infer this by checking the current image handle and (cheaply) trusting settings.
    if (brdfLUTImage_ != VK_NULL_HANDLE && brdfLUTSampler_ != VK_NULL_HANDLE && brdfLUTImageView_ != VK_NULL_HANDLE && brdfLUTCurrentSize_ == settings_.brdfLUTSize && settings_.brdfLUTSize > 1)
    {
      return;
    }

    // If fallback exists, release it and create a proper LUT.
    ibl_detail::deferDestroySampler(device_, brdfLUTSampler_);
    ibl_detail::deferDestroyImageView(device_, brdfLUTImageView_);
    ibl_detail::deferDestroyImage(device_, brdfLUTImage_);
    ibl_detail::deferFreeMemory(device_, brdfLUTMemory_);
    brdfLUTCurrentSize_ = 0;

    createBRDFLUT();
    createBRDFResources();
    generateBRDFLUT();
  }

  void IBLSystem::createIrradianceMap()
  {
    ibl_detail::createImage(device_,
                            settings_.irradianceSize,
                            settings_.irradianceSize,
                            1,
                            VK_FORMAT_R32G32B32A32_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            irradianceImage_,
                            irradianceMemory_,
                            6,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    irradianceImageView_ = ibl_detail::createImageView(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device_.getProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 1.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &irradianceSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance sampler!");
    }

    // Transition to color attachment optimal
    ibl_detail::transitionImageLayout(device_, irradianceImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 6);
  }

  void IBLSystem::createPrefilteredEnvMap()
  {
    uint32_t const mipLevels = static_cast<uint32_t>((std::max)(1, settings_.prefilterMipLevels));

    ibl_detail::createImage(device_,
                            settings_.prefilterSize,
                            settings_.prefilterSize,
                            mipLevels,
                            VK_FORMAT_R16G16B16A16_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            prefilteredImage_,
                            prefilteredMemory_,
                            6,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    prefilteredImageView_ = ibl_detail::createImageView(device_, prefilteredImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device_.getProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(mipLevels);

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &prefilteredSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter sampler!");
    }

    // Transition to color attachment optimal
    ibl_detail::transitionImageLayout(device_, prefilteredImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, mipLevels, 6);
  }

  void IBLSystem::createBRDFLUT()
  {
    ibl_detail::createImage(device_,
                            settings_.brdfLUTSize,
                            settings_.brdfLUTSize,
                            1,
                            VK_FORMAT_R16G16B16A16_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            brdfLUTImage_,
                            brdfLUTMemory_);

    brdfLUTImageView_ = ibl_detail::createImageView(device_, brdfLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1.0f;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 1.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &brdfLUTSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF LUT sampler!");
    }

    brdfLUTCurrentSize_ = settings_.brdfLUTSize;

    // Transition to general layout for compute shader storage
    ibl_detail::transitionImageLayout(device_, brdfLUTImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
  }

  void IBLSystem::createIrradianceResources()
  {
    // Regeneration path can call this multiple times; make it idempotent.
    ibl_detail::deferDestroyPipeline(device_, irradiancePipeline_);
    ibl_detail::deferDestroyPipelineLayout(device_, irradiancePipelineLayout_);
    ibl_detail::deferDestroyRenderPass(device_, irradianceRenderPass_);
    ibl_detail::deferDestroyDescriptorPool(device_, irradianceDescPool_);
    ibl_detail::deferDestroyDescriptorSetLayout(device_, irradianceDescSetLayout_);
    irradianceDescSet_ = VK_NULL_HANDLE;

    // Render Pass
    VkAttachmentDescription attachment{};
    attachment.format         = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &attachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &irradianceRenderPass_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance render pass!");
    }

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &irradianceDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance descriptor set layout!");
    }

    // Pipeline Layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4) + sizeof(int) + sizeof(float); // ViewProj + FaceIndex + SampleDelta

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &irradianceDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &irradiancePipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance pipeline layout!");
    }

    // Pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass                        = irradianceRenderPass_;
    pipelineConfig.pipelineLayout                    = irradiancePipelineLayout_;
    pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE; // No culling for full screen quad/cube
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    // No vertex input needed (generated in shader)
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    auto vertCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.vert.spv)");
    auto fragCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.frag.spv)");

    VkShaderModule vertModule;
    VkShaderModule fragModule;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &vertModule);

    createInfo.codeSize = fragCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &fragModule);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module              = vertModule;
    shaderStages[0].pName               = "main";
    shaderStages[0].flags               = 0;
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[0].pNext               = nullptr;

    shaderStages[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module              = fragModule;
    shaderStages[1].pName               = "main";
    shaderStages[1].flags               = 0;
    shaderStages[1].pSpecializationInfo = nullptr;
    shaderStages[1].pNext               = nullptr;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
    pipelineInfo.pViewportState      = &pipelineConfig.viewportInfo;
    pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
    pipelineInfo.pMultisampleState   = &pipelineConfig.multisampleInfo;
    pipelineInfo.pColorBlendState    = &pipelineConfig.colorBlendInfo;
    pipelineInfo.pDepthStencilState  = &pipelineConfig.depthStencilInfo;
    pipelineInfo.pDynamicState       = &pipelineConfig.dynamicStateInfo;
    pipelineInfo.layout              = irradiancePipelineLayout_;
    pipelineInfo.renderPass          = irradianceRenderPass_;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &irradiancePipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance pipeline!");
    }

    vkDestroyShaderModule(device_.device(), vertModule, nullptr);
    vkDestroyShaderModule(device_.device(), fragModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &irradianceDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = irradianceDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &irradianceDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &irradianceDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate irradiance descriptor set!");
    }
  }

  void IBLSystem::generateIrradianceMap(Skybox& skybox)
  {
    // Update descriptor set
    VkDescriptorImageInfo const imageInfo = skybox.getDescriptorInfo();
    VkWriteDescriptorSet        descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = irradianceDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    glm::mat4 const captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 const captureViews[]    = {glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),   // Top
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)), // Bottom
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkImageView>   imageViews;

    for (int i = 0; i < 6; ++i)
    {
      // Create view for this face
      VkImageView           faceView;
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = irradianceImage_;
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = 1;
      viewInfo.subresourceRange.baseArrayLayer = i;
      viewInfo.subresourceRange.layerCount     = 1;

      vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView);
      imageViews.push_back(faceView);

      // Create framebuffer
      VkFramebuffer           framebuffer;
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = irradianceRenderPass_;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments    = &faceView;
      framebufferInfo.width           = settings_.irradianceSize;
      framebufferInfo.height          = settings_.irradianceSize;
      framebufferInfo.layers          = 1;

      vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer);
      framebuffers.push_back(framebuffer);

      // Render
      VkRenderPassBeginInfo renderPassInfo{};
      renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass        = irradianceRenderPass_;
      renderPassInfo.framebuffer       = framebuffer;
      renderPassInfo.renderArea.offset = {0, 0};
      renderPassInfo.renderArea.extent = {static_cast<uint32_t>(settings_.irradianceSize), static_cast<uint32_t>(settings_.irradianceSize)};

      VkClearValue const clearValue  = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues    = &clearValue;

      vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport{};
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = static_cast<float>(settings_.irradianceSize);
      viewport.height   = static_cast<float>(settings_.irradianceSize);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.offset = {0, 0};
      scissor.extent = {static_cast<uint32_t>(settings_.irradianceSize), static_cast<uint32_t>(settings_.irradianceSize)};
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline_);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipelineLayout_, 0, 1, &irradianceDescSet_, 0, nullptr);

      struct PushBlock
      {
        glm::mat4 mvp;
        int       faceIndex;
        float     sampleDelta;
      } pushBlock;

      pushBlock.mvp         = captureProjection * captureViews[i];
      pushBlock.faceIndex   = i;
      pushBlock.sampleDelta = settings_.irradianceSampleDelta;

      vkCmdPushConstants(commandBuffer, irradiancePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

      vkCmdDraw(commandBuffer, 36, 1, 0, 0);

      vkCmdEndRenderPass(commandBuffer);
    }

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device_.device(), framebuffer, nullptr);
    }
    for (auto imageView : imageViews)
    {
      vkDestroyImageView(device_.device(), imageView, nullptr);
    }

    // Transition to shader read
    ibl_detail::transitionImageLayout(device_, irradianceImage_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);
  }

  void IBLSystem::createPrefilterResources()
  {
    // Regeneration path can call this multiple times; make it idempotent.
    ibl_detail::deferDestroyPipeline(device_, prefilterPipeline_);
    ibl_detail::deferDestroyPipelineLayout(device_, prefilterPipelineLayout_);
    ibl_detail::deferDestroyRenderPass(device_, prefilterRenderPass_);
    ibl_detail::deferDestroyDescriptorPool(device_, prefilterDescPool_);
    ibl_detail::deferDestroyDescriptorSetLayout(device_, prefilterDescSetLayout_);
    prefilterDescSet_ = VK_NULL_HANDLE;

    // Similar to Irradiance but different format and shader
    VkAttachmentDescription attachment{};
    attachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &attachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &prefilterRenderPass_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter render pass!");
    }

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &prefilterDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter descriptor set layout!");
    }

    // Pipeline Layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4) + sizeof(int) + sizeof(float) + sizeof(uint32_t); // ViewProj + FaceIndex + Roughness + SampleCount

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &prefilterDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &prefilterPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter pipeline layout!");
    }

    // Pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass                        = prefilterRenderPass_;
    pipelineConfig.pipelineLayout                    = prefilterPipelineLayout_;
    pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    auto vertCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(prefilter_envmap.vert.spv)");
    auto fragCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(prefilter_envmap.frag.spv)");

    VkShaderModule vertModule;
    VkShaderModule fragModule;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &vertModule);

    createInfo.codeSize = fragCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &fragModule);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module              = vertModule;
    shaderStages[0].pName               = "main";
    shaderStages[0].flags               = 0;
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[0].pNext               = nullptr;

    shaderStages[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module              = fragModule;
    shaderStages[1].pName               = "main";
    shaderStages[1].flags               = 0;
    shaderStages[1].pSpecializationInfo = nullptr;
    shaderStages[1].pNext               = nullptr;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
    pipelineInfo.pViewportState      = &pipelineConfig.viewportInfo;
    pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
    pipelineInfo.pMultisampleState   = &pipelineConfig.multisampleInfo;
    pipelineInfo.pColorBlendState    = &pipelineConfig.colorBlendInfo;
    pipelineInfo.pDepthStencilState  = &pipelineConfig.depthStencilInfo;
    pipelineInfo.pDynamicState       = &pipelineConfig.dynamicStateInfo;
    pipelineInfo.layout              = prefilterPipelineLayout_;
    pipelineInfo.renderPass          = prefilterRenderPass_;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &prefilterPipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter pipeline!");
    }

    vkDestroyShaderModule(device_.device(), vertModule, nullptr);
    vkDestroyShaderModule(device_.device(), fragModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &prefilterDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = prefilterDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &prefilterDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &prefilterDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate prefilter descriptor set!");
    }
  }

  void IBLSystem::generatePrefilteredEnvMap(Skybox& skybox)
  {
    // Update descriptor set
    VkDescriptorImageInfo const imageInfo = skybox.getDescriptorInfo();
    VkWriteDescriptorSet        descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = prefilterDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    glm::mat4 const captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 const captureViews[]    = {glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkImageView>   imageViews;

    int const mipCount = (std::max)(1, settings_.prefilterMipLevels);
    for (int mip = 0; mip < mipCount; ++mip)
    {
      auto const     baseSize  = static_cast<uint32_t>(settings_.prefilterSize);
      uint32_t const divisor   = 1u << static_cast<uint32_t>(mip);
      uint32_t const mipWidth  = (std::max)(1u, baseSize / divisor);
      uint32_t const mipHeight = (std::max)(1u, baseSize / divisor);
      float const    roughness = (mipCount <= 1) ? 0.0f : (static_cast<float>(mip) / static_cast<float>(mipCount - 1));

      for (int i = 0; i < 6; ++i)
      {
        VkImageView           faceView;
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = prefilteredImage_;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = mip;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount     = 1;

        vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView);
        imageViews.push_back(faceView);

        VkFramebuffer           framebuffer;
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = prefilterRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = &faceView;
        framebufferInfo.width           = mipWidth;
        framebufferInfo.height          = mipHeight;
        framebufferInfo.layers          = 1;

        vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer);
        framebuffers.push_back(framebuffer);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = prefilterRenderPass_;
        renderPassInfo.framebuffer       = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {mipWidth, mipHeight};

        VkClearValue const clearValue  = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues    = &clearValue;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(mipWidth);
        viewport.height   = static_cast<float>(mipHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {mipWidth, mipHeight};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterPipelineLayout_, 0, 1, &prefilterDescSet_, 0, nullptr);

        struct PushBlock
        {
          glm::mat4 mvp;
          int       faceIndex;
          float     roughness;
          uint32_t  sampleCount;
        } pushBlock;

        pushBlock.mvp         = captureProjection * captureViews[i];
        pushBlock.faceIndex   = i;
        pushBlock.roughness   = roughness;
        pushBlock.sampleCount = settings_.prefilterSampleCount;

        vkCmdPushConstants(commandBuffer, prefilterPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

        vkCmdDraw(commandBuffer, 36, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
      }
    }

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device_.device(), framebuffer, nullptr);
    }
    for (auto imageView : imageViews)
    {
      vkDestroyImageView(device_.device(), imageView, nullptr);
    }

    ibl_detail::transitionImageLayout(device_, prefilteredImage_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, static_cast<uint32_t>(mipCount), 6);
  }

  void IBLSystem::createBRDFResources()
  {
    // BRDF LUT size may change; make this idempotent to avoid leaks.
    ibl_detail::deferDestroyPipeline(device_, brdfPipeline_);
    ibl_detail::deferDestroyPipelineLayout(device_, brdfPipelineLayout_);
    ibl_detail::deferDestroyDescriptorPool(device_, brdfDescPool_);
    ibl_detail::deferDestroyDescriptorSetLayout(device_, brdfDescSetLayout_);
    brdfDescSet_ = VK_NULL_HANDLE;

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &brdfDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF descriptor set layout!");
    }

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &brdfDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &brdfPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF pipeline layout!");
    }

    // Compute Pipeline
    auto compCode = Pipeline::readFile(std::string(SHADER_PATH) + "brdf_lut.comp.spv");

    VkShaderModule           compModule;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = compCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(compCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &compModule);

    VkPipelineShaderStageCreateInfo shaderStage{};
    shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = compModule;
    shaderStage.pName  = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = shaderStage;
    pipelineInfo.layout = brdfPipelineLayout_;

    if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &brdfPipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF compute pipeline!");
    }

    vkDestroyShaderModule(device_.device(), compModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &brdfDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = brdfDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &brdfDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &brdfDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate BRDF descriptor set!");
    }
  }

  void IBLSystem::generateBRDFLUT()
  {
    // Update descriptor set
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView   = brdfLUTImageView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = brdfDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipelineLayout_, 0, 1, &brdfDescSet_, 0, nullptr);

    vkCmdDispatch(commandBuffer, settings_.brdfLUTSize / 16, settings_.brdfLUTSize / 16, 1);

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    ibl_detail::transitionImageLayout(device_, brdfLUTImage_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  }

} // namespace engine
