#include "Engine/Systems/ProceduralSkyCapture.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {
    ProceduralSkyCapture::ProceduralSkyCapture(Device& device) : device_{device} {}
    ProceduralSkyCapture::~ProceduralSkyCapture() {
        VkDevice dev = device_.device();
        for (auto fb : framebuffers_) {
            vkDestroyFramebuffer(dev, fb, nullptr);
        }
        for (auto view : faceViews_) {
            vkDestroyImageView(dev, view, nullptr);
        }
        pipeline_.reset();
        if (layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, layout_, nullptr);
        }
        if (renderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(dev, renderPass_, nullptr);
        }
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, pool_, nullptr);
        }
        if (setLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, setLayout_, nullptr);
        }
        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(dev, sampler_, nullptr);
        }
    }

    void ProceduralSkyCapture::ensurePipeline(uint32_t faceSize) {
        if (pipeline_ != nullptr && targetSize_ == faceSize) {
            return;
        }
        // Tear down previous targets (size changed).
        for (auto fb : framebuffers_) {
            vkDestroyFramebuffer(device_.device(), fb, nullptr);
        }
        for (auto view : faceViews_) {
            vkDestroyImageView(device_.device(), view, nullptr);
        }
        framebuffers_.clear();
        faceViews_.clear();

        pipeline_.reset();
        if (layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), layout_, nullptr);
        }
        if (renderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_.device(), renderPass_, nullptr);
        }
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_.device(), pool_, nullptr);
        }
        if (setLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), setLayout_, nullptr);
        }
        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_.device(), sampler_, nullptr);
        }
        pipeline_.reset();
        layout_     = VK_NULL_HANDLE;
        renderPass_ = VK_NULL_HANDLE;
        pool_       = VK_NULL_HANDLE;
        setLayout_  = VK_NULL_HANDLE;
        sampler_    = VK_NULL_HANDLE;

        // --- Render pass: one RGBA16F color attachment, no depth ---
        VkAttachmentDescription attachment{};
        attachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments    = &attachment;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        if (vkCreateRenderPass(device_.device(), &rpInfo, nullptr, &renderPass_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to create render pass");
        }

        // --- Descriptor set: none needed (push constants only) ---
        VkDescriptorSetLayoutCreateInfo slInfo{};
        slInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        slInfo.bindingCount = 0;
        if (vkCreateDescriptorSetLayout(device_.device(), &slInfo, nullptr, &setLayout_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to create descriptor set layout");
        }

        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset     = 0;
        pc.size       = sizeof(SkyboxPushConstants);
        VkPipelineLayoutCreateInfo plInfo{};
        plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plInfo.setLayoutCount         = 1;
        plInfo.pSetLayouts            = &setLayout_;
        plInfo.pushConstantRangeCount = 1;
        plInfo.pPushConstantRanges    = &pc;
        if (vkCreatePipelineLayout(device_.device(), &plInfo, nullptr, &layout_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to create pipeline layout");
        }

        PipelineConfigInfo config{};
        Pipeline::defaultPipelineConfigInfo(config);
        config.renderPass                        = renderPass_;
        config.pipelineLayout                    = layout_;
        config.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
        config.depthStencilInfo.depthTestEnable  = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        config.bindingDescriptions.clear();
        config.attributeDescriptions.clear();
        pipeline_ = std::make_unique<Pipeline>(device_,
            std::string(SHADER_PATH) + "skybox_fullscreen.vert.spv",
            std::string(SHADER_PATH) + "skybox_fullscreen.frag.spv",
            config);

        VkDescriptorPoolSize       poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0};
        VkDescriptorPoolCreateInfo dpInfo{};
        dpInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpInfo.maxSets       = 1;
        dpInfo.poolSizeCount = 0;
        dpInfo.pPoolSizes    = &poolSize;
        if (vkCreateDescriptorPool(device_.device(), &dpInfo, nullptr, &pool_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to create descriptor pool");
        }
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = pool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &setLayout_;
        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &descSet_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to allocate descriptor set");
        }

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
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;
        if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
            throw std::runtime_error("ProceduralSkyCapture: failed to create sampler");
        }

        targetSize_ = faceSize;
    }

    void ProceduralSkyCapture::ensureRenderTargets(uint32_t faceSize) {
        if (!target_ || targetSize_ != faceSize) {
            target_ = std::make_unique<Skybox>(device_, faceSize);
            // Skybox(size) leaves the cube in UNDEFINED; the render pass expects
            // COLOR_ATTACHMENT_OPTIMAL. Transition once after creation.
            ibl_detail::transitionImageLayout(device_, target_->getImage(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 6);
            targetTransitioned_ = true;
            // Build face views + framebuffers for the 6 cube faces.
            for (auto fb : framebuffers_) {
                vkDestroyFramebuffer(device_.device(), fb, nullptr);
            }
            for (auto view : faceViews_) {
                vkDestroyImageView(device_.device(), view, nullptr);
            }
            framebuffers_.clear();
            faceViews_.clear();

            for (uint32_t i = 0; i < 6; ++i) {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image                           = target_->getImage();
                viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
                viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel   = 0;
                viewInfo.subresourceRange.levelCount     = 1;
                viewInfo.subresourceRange.baseArrayLayer = i;
                viewInfo.subresourceRange.layerCount     = 1;
                VkImageView faceView                     = VK_NULL_HANDLE;
                if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView) != VK_SUCCESS) {
                    throw std::runtime_error("ProceduralSkyCapture: failed to create face view");
                }
                faceViews_.push_back(faceView);

                VkFramebufferCreateInfo fbInfo{};
                fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fbInfo.renderPass      = renderPass_;
                fbInfo.attachmentCount = 1;
                fbInfo.pAttachments    = &faceView;
                fbInfo.width           = faceSize;
                fbInfo.height          = faceSize;
                fbInfo.layers          = 1;
                VkFramebuffer fb       = VK_NULL_HANDLE;
                if (vkCreateFramebuffer(device_.device(), &fbInfo, nullptr, &fb) != VK_SUCCESS) {
                    throw std::runtime_error("ProceduralSkyCapture: failed to create framebuffer");
                }
                framebuffers_.push_back(fb);
            }
        }
    }

    Skybox* ProceduralSkyCapture::capture(const SkyboxSettings& settings, uint32_t faceSize) {
        ensurePipeline(faceSize);
        ensureRenderTargets(faceSize);

        const glm::vec3 sunDir = sunDirectionFromTimeOfDay(settings.timeOfDay, settings.latitude, static_cast<float>(settings.dayOfYear));

        // Physically-derived sun colour (matches the LUT extinction).
        glm::vec3 sunCol = computeSunDirectColor(sunDir,
            static_cast<float>(settings.atmosphereRadius),
            glm::max(glm::vec3(settings.betaRayleigh), glm::vec3(0.0f)),
            glm::max(glm::vec3(settings.betaMie), glm::vec3(0.0f)),
            static_cast<float>(settings.rayleighScaleHeight),
            static_cast<float>(settings.mieScaleHeight));
        if (glm::all(glm::equal(sunCol, glm::vec3(0.0f)))) {
            sunCol = glm::vec3(1.0f, 0.35f, 0.1f);
        }
        const float nightFactor        = glm::smoothstep(-0.05f, 0.15f, sunDir.y);
        const float effectiveIntensity = settings.skyIntensity * glm::mix(0.02f, 1.0f, nightFactor);

        // Face order MUST match cubeDir() in skybox_fullscreen.frag:
        // 0:+Z 1:-Z 2:+Y 3:-Y 4:+X 5:-X
        constexpr int   kFaces               = 6;
        const glm::mat4 captureViews[kFaces] = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        };
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        SkyboxPushConstants push{};
        // viewPosition unused in capture; identity-ish. Capture branch ignores vp.
        push.viewProjection = glm::mat4(1.0f);
        push.debugParams    = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);  // w=1 -> capture mode
        push.sunDirection   = glm::vec4(sunDir, 0.0f);
        push.sunColor       = glm::vec4(sunCol, 0.015f);
        push.skyParams      = glm::vec4(settings.timeOfDay, effectiveIntensity, 0.0f, 0.0f);

        VkCommandBuffer cmd = device_.memory().beginSingleTimeCommands();
        for (int i = 0; i < kFaces; ++i) {
            VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass        = renderPass_;
            rpBegin.framebuffer       = framebuffers_[i];
            rpBegin.renderArea.offset = {0, 0};
            rpBegin.renderArea.extent = {faceSize, faceSize};
            VkClearValue clearValue{};
            clearValue.color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
            rpBegin.clearValueCount = 1;
            rpBegin.pClearValues    = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{};
            viewport.width    = static_cast<float>(faceSize);
            viewport.height   = static_cast<float>(faceSize);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.extent = {faceSize, faceSize};
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->getPipeline());
            push.faceIndex = i;
            vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(SkyboxPushConstants), &push);
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }
        device_.memory().endSingleTimeCommands(cmd);

        // Transition the captured cubemap to shader-read layout for IBL sampling.
        // Skybox ctor created it in COLOR_ATTACHMENT_OPTIMAL via its own path;
        // ensure SHADER_READ_ONLY for the IBL convolution/prefilter reads.
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = target_->getImage();
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6;
        barrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

        VkCommandBuffer barrierCmd = device_.memory().beginSingleTimeCommands();
        vkCmdPipelineBarrier(barrierCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        device_.memory().endSingleTimeCommands(barrierCmd);

        return target_.get();
    }
}  // namespace engine
