#include "Engine/Systems/IBL/IrradianceIBL.hpp"

#include <stdexcept>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
namespace engine::ibl {
    IrradianceIBL::IrradianceIBL(Device& device) : device_(device) {}
    IrradianceIBL::~IrradianceIBL() = default;
    void IrradianceIBL::resetToUninitialized() {
        image_          = VK_NULL_HANDLE;
        memory_         = VK_NULL_HANDLE;
        imageView_      = VK_NULL_HANDLE;
        sampler_        = VK_NULL_HANDLE;
        renderPass_     = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        pipeline_       = VK_NULL_HANDLE;
        descSetLayout_  = VK_NULL_HANDLE;
        descPool_       = VK_NULL_HANDLE;
        descSet_        = VK_NULL_HANDLE;
    }
    VkDescriptorImageInfo IrradianceIBL::getDescriptorInfo() const {
        return VkDescriptorImageInfo{
            .sampler     = sampler_,
            .imageView   = imageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    void IrradianceIBL::adoptLoaded(VkImage image, VkDeviceMemory memory, VkImageView imageView, VkSampler sampler) {
        image_     = image;
        memory_    = memory;
        imageView_ = imageView;
        sampler_   = sampler;
    }
    void IrradianceIBL::deferDestroyImageResources() {
        ibl_detail::deferDestroySampler(device_, sampler_);
        ibl_detail::deferDestroyImageView(device_, imageView_);
        ibl_detail::deferDestroyImage(device_, image_);
        ibl_detail::deferFreeMemory(device_, memory_);
    }
    void IrradianceIBL::destroyImmediate() {
        VkDevice dev = device_.device();
        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(dev, sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }
        if (imageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(dev, image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(dev, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(dev, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }
        if (renderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(dev, renderPass_, nullptr);
            renderPass_ = VK_NULL_HANDLE;
        }
        if (descPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, descPool_, nullptr);
            descPool_ = VK_NULL_HANDLE;
        }
        if (descSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, descSetLayout_, nullptr);
            descSetLayout_ = VK_NULL_HANDLE;
        }
        descSet_ = VK_NULL_HANDLE;
    }
    void IrradianceIBL::createFallback() {
        ibl_detail::createImage(device_,
            1,
            1,
            1,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image_,
            memory_,
            6,
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        imageView_ = ibl_detail::createImageView(device_, image_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
        {
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
            samplerInfo.maxLod                  = 0.0f;
            if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
                throw std::runtime_error("failed to create fallback irradiance sampler!");
            }
        }
        VkClearColorValue const clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};
        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 6);
        VkCommandBuffer         cmd = device_.getMemory().beginSingleTimeCommands();
        VkImageSubresourceRange range{};
        range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = 1;
        range.baseArrayLayer = 0;
        range.layerCount     = 6;
        vkCmdClearColorImage(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
        device_.getMemory().endSingleTimeCommands(cmd);
        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);
    }
    void IrradianceIBL::createForSettings(const Settings& settings) {
        ibl_detail::createImage(device_,
            settings.irradianceSize,
            settings.irradianceSize,
            1,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image_,
            memory_,
            6,
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        imageView_ = ibl_detail::createImageView(device_, image_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
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
        if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance sampler!");
        }
        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 6);
    }
    void IrradianceIBL::ensurePipelineResources() {
        ibl_detail::deferDestroyPipeline(device_, pipeline_);
        ibl_detail::deferDestroyPipelineLayout(device_, pipelineLayout_);
        ibl_detail::deferDestroyRenderPass(device_, renderPass_);
        ibl_detail::deferDestroyDescriptorPool(device_, descPool_);
        ibl_detail::deferDestroyDescriptorSetLayout(device_, descSetLayout_);
        descSet_ = VK_NULL_HANDLE;
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
        if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance render pass!");
        }
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
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance descriptor set layout!");
        }
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(glm::mat4) + sizeof(int) + sizeof(float);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &descSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
        if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance pipeline layout!");
        }
        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass                        = renderPass_;
        pipelineConfig.pipelineLayout                    = pipelineLayout_;
        pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        auto                     vertCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.vert.spv)");
        auto                     fragCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.frag.spv)");
        VkShaderModule           vertModule;
        VkShaderModule           fragModule;
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
        vertexInputInfo.sType            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
        pipelineInfo.pViewportState      = &pipelineConfig.viewportInfo;
        pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
        pipelineInfo.pMultisampleState   = &pipelineConfig.multisampleInfo;
        pipelineInfo.pColorBlendState    = &pipelineConfig.colorBlendInfo;
        pipelineInfo.pDepthStencilState  = &pipelineConfig.depthStencilInfo;
        pipelineInfo.pDynamicState       = &pipelineConfig.dynamicStateInfo;
        pipelineInfo.layout              = pipelineLayout_;
        pipelineInfo.renderPass          = renderPass_;
        pipelineInfo.subpass             = 0;
        if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance pipeline!");
        }
        vkDestroyShaderModule(device_.device(), vertModule, nullptr);
        vkDestroyShaderModule(device_.device(), fragModule, nullptr);
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 1;
        if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &descPool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create irradiance descriptor pool!");
        }
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descSetLayout_;
        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &descSet_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate irradiance descriptor set!");
        }
    }
    void IrradianceIBL::generateFromSkybox(Skybox& skybox, const Settings& settings) {
        VkDescriptorImageInfo const imageInfo = skybox.getDescriptorInfo();
        VkWriteDescriptorSet        descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = descSet_;
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);
        glm::mat4 const            captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 const            captureViews[]    = {glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};
        VkCommandBuffer            commandBuffer     = device_.getMemory().beginSingleTimeCommands();
        std::vector<VkFramebuffer> framebuffers;
        std::vector<VkImageView>   imageViews;
        for (int i = 0; i < 6; ++i) {
            VkImageView           faceView;
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = image_;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = i;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView);
            imageViews.push_back(faceView);
            VkFramebuffer           framebuffer;
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = renderPass_;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments    = &faceView;
            framebufferInfo.width           = static_cast<uint32_t>(settings.irradianceSize);
            framebufferInfo.height          = static_cast<uint32_t>(settings.irradianceSize);
            framebufferInfo.layers          = 1;
            vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer);
            framebuffers.push_back(framebuffer);
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass        = renderPass_;
            renderPassInfo.framebuffer       = framebuffer;
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = {static_cast<uint32_t>(settings.irradianceSize), static_cast<uint32_t>(settings.irradianceSize)};
            VkClearValue const clearValue    = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            renderPassInfo.clearValueCount   = 1;
            renderPassInfo.pClearValues      = &clearValue;
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{};
            viewport.x        = 0.0f;
            viewport.y        = 0.0f;
            viewport.width    = static_cast<float>(settings.irradianceSize);
            viewport.height   = static_cast<float>(settings.irradianceSize);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {static_cast<uint32_t>(settings.irradianceSize), static_cast<uint32_t>(settings.irradianceSize)};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descSet_, 0, nullptr);
            struct PushBlock {
                glm::mat4 mvp;
                int       faceIndex;
                float     sampleDelta;
            } pushBlock;
            pushBlock.mvp         = captureProjection * captureViews[i];
            pushBlock.faceIndex   = i;
            pushBlock.sampleDelta = settings.irradianceSampleDelta;
            vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);
            vkCmdDraw(commandBuffer, 36, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffer);
        }
        device_.getMemory().endSingleTimeCommands(commandBuffer);
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device_.device(), framebuffer, nullptr);
        }
        for (auto imageView : imageViews) {
            vkDestroyImageView(device_.device(), imageView, nullptr);
        }
        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);
    }
}  // namespace engine::ibl
