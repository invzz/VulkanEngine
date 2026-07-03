#include "Engine/Systems/SelectionOutlineSystem.hpp"

#include <vulkan/vulkan_core.h>

#include <entt/entity/fwd.hpp>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    SelectionOutlineSystem::SelectionOutlineSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : device_(device) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(glm::vec3) * 3;

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

        if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create selection outline pipeline layout");
        }

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout_;
        pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_TRUE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipeline_ = std::make_unique<Pipeline>(
            device_,
            std::string(SHADER_PATH) + "debug_selection_outline.vert.spv",
            std::string(SHADER_PATH) + "debug_selection_outline.frag.spv",
            pipelineConfig);
    }

    SelectionOutlineSystem::~SelectionOutlineSystem() {
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        }
    }

    void SelectionOutlineSystem::render(FrameInfo& frameInfo) const {
        entt::entity selectedEntity = frameInfo.selectedEntity;
        if (selectedEntity == entt::null) {
            return;
        }

        const auto& registry = frameInfo.scene->getRegistry();
        if (!registry.all_of<TransformComponent>(selectedEntity) ||
            !registry.all_of<ModelComponent>(selectedEntity)) {
            return;
        }

        const auto& transform = registry.get<TransformComponent>(selectedEntity);
        const auto& modelComp = registry.get<ModelComponent>(selectedEntity);
        const auto* model     = modelComp.model.get();

        if (!model || !model->getLocalBounds().isValid()) {
            return;
        }

        AABB worldBounds = transformAABB(model->getLocalBounds(), transform.modelTransform());

        pipeline_->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "SelectionOutlineSystem: global descriptor set is null");
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        glm::vec3 pushData[3];
        pushData[0] = worldBounds.min;
        pushData[1] = worldBounds.max;
        pushData[2] = glm::vec3(1.0f, 0.0f, 0.0f);

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(pushData),
            &pushData);

        vkCmdDraw(frameInfo.commandBuffer, 24, 1, 0, 0);
    }

}  // namespace engine
