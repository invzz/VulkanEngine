#include "Engine/Graphics/ViewportDisplay.hpp"

#include <vulkan/vulkan_core.h>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/ViewportTexture.hpp"

namespace engine {

    ViewportDisplay::~ViewportDisplay() {
        if (pipeline_ != nullptr) {
            pipeline_ = nullptr;
        }
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_ ? device_->device() : VK_NULL_HANDLE, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_ ? device_->device() : VK_NULL_HANDLE, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_ ? device_->device() : VK_NULL_HANDLE, descriptorSetLayout_, nullptr);
            descriptorSetLayout_ = VK_NULL_HANDLE;
        }
        if (displayRenderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_ ? device_->device() : VK_NULL_HANDLE, displayRenderPass_, nullptr);
            displayRenderPass_ = VK_NULL_HANDLE;
        }
    }

    void ViewportDisplay::initialize(Device& device,
                                     VkRenderPass swapChainRenderPass,
                                     VkFormat swapChainFormat,
                                     VkExtent2D swapChainExtent) {
        device_                = &device;
        swapChainRenderPass_   = swapChainRenderPass;
        swapChainFormat_       = swapChainFormat;
        swapChainExtent_       = swapChainExtent;

        createRenderPass(device);
        createDescriptorInfrastructure(device);
        createPipeline(device);
    }

    void ViewportDisplay::setViewportTexture(const ViewportTexture& viewportTexture) {
        viewportTexture_ = &viewportTexture;

        // Update descriptor set with the viewport texture's image info
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler     = viewportTexture_->getSampler();
        imageInfo.imageView   = viewportTexture_->getImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet           = descriptorSet_;
        write.dstBinding       = 0;
        write.dstArrayElement  = 0;
        write.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount  = 1;
        write.pImageInfo       = &imageInfo;

        vkUpdateDescriptorSets(device_->device(), 1, &write, 0, nullptr);
    }

    void ViewportDisplay::setRenderPass(VkRenderPass renderPass) {
        swapChainRenderPass_ = renderPass;
    }

    void ViewportDisplay::execute(VkCommandBuffer commandBuffer, VkFramebuffer swapChainFramebuffer) const {
        if (!isValid() || !viewportTexture_) {
            return;
        }

        // 1. Begin render pass using swap chain render pass (2 attachments: color + depth)
        //    Color attachment has LOAD op (preserves viewport), depth has CLEAR op
        VkClearValue clearValues[] = {
            {{0.0f, 0.0f, 0.0f, 1.0f}},
            {1.0f, 0},
        };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType         = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass    = displayRenderPass_;  // Must match the pipeline's render pass (1 attachment)
        renderPassInfo.framebuffer   = swapChainFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent_;
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues  = clearValues;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 2. Set viewport and scissor
        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(swapChainExtent_.width);
        viewport.height   = static_cast<float>(swapChainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // 3. Bind pipeline and descriptor set
        pipeline_->bind(commandBuffer);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

        // 4. Render fullscreen quad (3 vertices, gl_VertexIndex-based)
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 5. End render pass
        vkCmdEndRenderPass(commandBuffer);
    }

    void ViewportDisplay::createRenderPass(Device& device) {
        // Color attachment matching swap chain format
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = swapChainFormat_;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;   // Preserve content
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;

        // Dependencies
        std::array<VkSubpassDependency, 2> dependencies{};

        // Previous render pass -> this render pass
        dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass      = 0;
        dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // This render pass -> next render pass
        dependencies[1].srcSubpass      = 0;
        dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies   = dependencies.data();

        if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &displayRenderPass_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport display render pass!");
        }
    }

    void ViewportDisplay::createDescriptorInfrastructure(Device& device) {
        // Create descriptor set layout (single sampler2D)
        DescriptorSetLayout::Builder layoutBuilder{device};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descriptorSetLayoutObj_ = layoutBuilder.build();
        descriptorSetLayout_    = descriptorSetLayoutObj_->getDescriptorSetLayout();

        // Create descriptor pool (create directly, no wrapper needed)
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport display descriptor pool!");
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout_;

        if (vkAllocateDescriptorSets(device.device(), &allocInfo, &descriptorSet_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to allocate viewport display descriptor set!");
        }
    }

    void ViewportDisplay::createPipeline(Device& device) {
        PipelineConfigInfo configInfo{};
        Pipeline::defaultPipelineConfigInfo(configInfo);

        // Create pipeline layout
        VkDescriptorSetLayout layouts[] = {descriptorSetLayout_};
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport display pipeline layout!");
        }

        configInfo.pipelineLayout = pipelineLayout_;
        configInfo.renderPass     = displayRenderPass_;
        configInfo.subpass        = 0;

        // Build pipeline
        pipeline_ = std::make_unique<Pipeline>(
            device,
            std::string(SHADER_PATH) + "viewport_display.vert.spv",
            std::string(SHADER_PATH) + "viewport_display.frag.spv",
            configInfo);
    }

}  // namespace engine
