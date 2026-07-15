#include "Engine/Systems/SelectionCompositeSystem.hpp"

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/SwapChain.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    SelectionCompositeSystem::SelectionCompositeSystem(Device& device, VkRenderPass renderPass, Renderer& renderer)
        : device_(device), renderer_(renderer) {
        // Two sampled textures: scene color (post-fx) and the selection mask.
        DescriptorSetLayout::Builder layoutBuilder(device_);
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descriptorSetLayout_ = layoutBuilder.build();
        VkDescriptorSetLayout rawLayout = descriptorSetLayout_->getDescriptorSetLayout();

        const uint32_t frameCount = SwapChain::maxFramesInFlight();
        descriptorPool_           = DescriptorPool::Builder(device_)
                                   .setMaxSets(frameCount)
                                   .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2)
                                   .build();
        descriptorSets_.resize(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i) {
            if (!descriptorPool_->allocateDescriptor(rawLayout, descriptorSets_[i])) {
                throw std::runtime_error("failed to allocate selection composite descriptor set");
            }
        }

        // Explicit pipeline layout (the composite shader has no push constants).
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount  = 1;
        layoutInfo.pSetLayouts     = &rawLayout;
        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create selection composite pipeline layout");
        }

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout_;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        pipeline_ = std::make_unique<Pipeline>(
            device,
            std::string(SHADER_PATH) + "post_process.vert.spv",
            std::string(SHADER_PATH) + "selection_composite.frag.spv",
            pipelineConfig);
    }

    SelectionCompositeSystem::~SelectionCompositeSystem() {
        vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        vkDestroyDescriptorSetLayout(device_.device(), descriptorSetLayout_->getDescriptorSetLayout(), nullptr);
    }

    void SelectionCompositeSystem::render(FrameInfo& frameInfo) const {
        const int frameIndex = renderer_.getFrameIndex();

        // (Re)write the post-fx + mask images into this frame's descriptor set.
        VkDescriptorImageInfo sceneInfo = renderer_.getPostFxImageInfo(frameIndex);
        VkDescriptorImageInfo maskInfo  = renderer_.getSelectionMaskImageInfo(frameIndex);
        DescriptorWriter writer(*descriptorSetLayout_, *descriptorPool_);
        writer.writeImage(0, &sceneInfo);
        writer.writeImage(1, &maskInfo);
        VkDescriptorSet set = descriptorSets_[frameIndex];
        writer.overwrite(set);

        pipeline_->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[frameIndex],
            0,
            nullptr);
        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
    }
}  // namespace engine
