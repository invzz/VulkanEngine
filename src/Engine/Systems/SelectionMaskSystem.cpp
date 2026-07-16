#include "Engine/Systems/SelectionMaskSystem.hpp"

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ChildComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NodeIndexComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "vulkan/vulkan_core.h"
namespace engine {
    namespace {
        // Mirror ModelRenderSystem::MeshPushConstantData exactly — the simple_mesh
        // mesh/task shaders expect this byte layout.
        struct MaskPushConstantData {
            glm::mat4 modelMatrix{1.0f};
            glm::mat4 normalMatrix{1.0f};
            uint32_t  meshId{0};
            uint64_t  meshletBufferAddress{0};
            uint64_t  meshletVerticesAddress{0};
            uint64_t  meshletTrianglesAddress{0};
            uint64_t  vertexBufferAddress{0};
            uint32_t  meshletOffset{0};
            uint32_t  meshletCount{0};
        };
        static_assert(sizeof(MaskPushConstantData) == 176, "MaskPushConstantData size must match MeshPushConstantData (176)");
        static_assert(offsetof(MaskPushConstantData, meshletOffset) == 168, "meshletOffset offset mismatch");
        static_assert(offsetof(MaskPushConstantData, meshletCount) == 172, "meshletCount offset mismatch");
    }  // namespace

    entt::entity SelectionMaskSystem::resolveModelEntity(const Scene& scene, entt::entity selected) {
        if (selected == entt::null || !scene.getRegistry().valid(selected)) {
            return entt::null;
        }
        const auto& registry = scene.getRegistry();
        if (registry.all_of<ModelComponent>(selected)) {
            return selected;
        }
        // A node (or sub-mesh) selection: walk up via ChildComponent to the owner model.
        entt::entity current = selected;
        int          guard   = 0;
        while (guard++ < 64) {
            if (!registry.valid(current)) {
                return entt::null;
            }
            if (registry.all_of<ModelComponent>(current)) {
                return current;
            }
            if (registry.all_of<ChildComponent>(current)) {
                current = registry.get<ChildComponent>(current).parent;
            } else {
                break;
            }
        }
        return entt::null;
    }

    SelectionMaskSystem::SelectionMaskSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : device_(device), globalSetLayout_(globalSetLayout) {
        // The mask pass reuses the same buffer-address push constants as the mesh
        // pipeline, so it needs the same push-constant range.
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(MaskPushConstantData);
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &globalSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushConstantRange;
        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create selection mask pipeline layout!");
        }
        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout_;
        pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;  // capture full silhouette
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;           // on top of all geometry
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        // Mesh shading pipeline: task + mesh + fragment shaders.
        pipeline_ = std::make_unique<Pipeline>(
            device,
            std::string(SHADER_PATH) + "simple_mesh.task.spv",
            std::string(SHADER_PATH) + "simple_mesh.mesh.spv",
            std::string(SHADER_PATH) + "selection_mask.frag.spv",
            pipelineConfig);
    }

    SelectionMaskSystem::~SelectionMaskSystem() {
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        }
    }

    void SelectionMaskSystem::render(FrameInfo& frameInfo) const {
        entt::entity modelEntity = resolveModelEntity(*frameInfo.scene, frameInfo.selectedEntity);
        if (modelEntity == entt::null) {
            return;
        }
        const auto& registry  = frameInfo.scene->getRegistry();
        const auto& transform = registry.get<TransformComponent>(modelEntity);
        const auto& modelComp = registry.get<ModelComponent>(modelEntity);
        const auto* model     = modelComp.model.get();
        if (!model || model->getMeshletCount() == 0) {
            return;
        }

        // Default: whole model. If the selection is a sub-mesh entity, restrict
        // to that sub-mesh's meshlet range. If it is a node (instance) entity,
        // restrict to the sub-mesh(es) that node instantiates — giving the
        // instance the same outline behaviour as selecting the sub-mesh directly.
        uint32_t    meshletOffset = 0;
        uint32_t    meshletCount  = model->getMeshletCount();
        const auto& subMeshes     = model->getSubMeshes();
        auto        sel           = frameInfo.selectedEntity;
        if (registry.valid(sel) && registry.all_of<SubMeshComponent>(sel)) {
            const auto& sub = registry.get<SubMeshComponent>(sel);
            if (sub.subMeshIndex < subMeshes.size()) {
                meshletOffset = subMeshes[sub.subMeshIndex].meshletOffset;
                meshletCount  = subMeshes[sub.subMeshIndex].meshletCount;
            }
        } else if (registry.valid(sel) && registry.all_of<NodeIndexComponent>(sel)) {
            const int nodeIdx    = registry.get<NodeIndexComponent>(sel).nodeIndex;
            uint32_t  rangeStart = model->getMeshletCount();
            uint32_t  rangeEnd   = 0;
            for (const auto& sm : subMeshes) {
                if (sm.nodeIndex == nodeIdx) {
                    rangeStart = std::min(rangeStart, sm.meshletOffset);
                    rangeEnd   = std::max(rangeEnd, sm.meshletOffset + sm.meshletCount);
                }
            }
            if (rangeEnd > rangeStart) {
                meshletOffset = rangeStart;
                meshletCount  = rangeEnd - rangeStart;
            }
        }

        MaskPushConstantData push{};
        push.modelMatrix             = transform.modelTransform();
        push.normalMatrix            = glm::transpose(glm::inverse(push.modelMatrix));
        push.meshId                  = model->getMeshId();
        push.meshletBufferAddress    = model->getMeshletBufferAddress();
        push.meshletVerticesAddress  = model->getMeshletVerticesAddress();
        push.meshletTrianglesAddress = model->getMeshletTrianglesAddress();
        push.vertexBufferAddress     = model->getVertexBufferAddress();
        push.meshletOffset           = meshletOffset;
        push.meshletCount            = meshletCount;

        pipeline_->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "SelectionMaskSystem: global descriptor set is null");
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);
        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(push),
            &push);
        const uint32_t groupCount = (push.meshletCount + 31u) / 32u;
        if (device_.vkCmdDrawMeshTasksEXT != nullptr) {
            device_.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);
        }
    }
}  // namespace engine
