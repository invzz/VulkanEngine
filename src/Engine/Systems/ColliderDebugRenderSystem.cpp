#include "Engine/Systems/ColliderDebugRenderSystem.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    namespace {
        constexpr uint32_t kBoxVertexCount     = 24;
        constexpr uint32_t kSphereSegmentCount = 24;
        constexpr uint32_t kSphereVertexCount  = 3 * (kSphereSegmentCount * 2);
        constexpr uint32_t kCapsuleVertexCount = (2 * kSphereSegmentCount * 2) + 8;
    }  // namespace

    ColliderDebugRenderSystem::ColliderDebugRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : device_(device) {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    ColliderDebugRenderSystem::~ColliderDebugRenderSystem() {
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        }
    }

    void ColliderDebugRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(PushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

        if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create collider debug pipeline layout");
        }
    }

    void ColliderDebugRenderSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout_ != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout_;
        pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipeline_ = std::make_unique<Pipeline>(
            device_,
            std::string(SHADER_PATH) + "debug_collider.vert.spv",
            std::string(SHADER_PATH) + "debug_collider.frag.spv",
            pipelineConfig);
    }

    glm::mat4 ColliderDebugRenderSystem::makeNoScaleModelMatrix(const TransformComponent& transform) {
        glm::mat4 model = glm::mat4(1.0f);
        model           = glm::translate(model, transform.translation);
        model           = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model           = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model           = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        return model;
    }

    void ColliderDebugRenderSystem::render(FrameInfo& frameInfo) const {
        pipeline_->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "ColliderDebugRenderSystem: global descriptor set is null");
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        auto& registry = frameInfo.scene->getRegistry();
        auto  view     = registry.view<ColliderComponent, TransformComponent>();

        for (auto entity : view) {
            auto [collider, transform] = view.get<ColliderComponent, TransformComponent>(entity);

            PushConstantData push{};
            push.modelMatrix = makeNoScaleModelMatrix(transform);
            push.modelMatrix = glm::translate(push.modelMatrix, collider.centerOffset);

            if (registry.all_of<RigidBodyComponent>(entity)) {
                const auto& rb = registry.get<RigidBodyComponent>(entity);
                if (rb.isStatic || rb.mode == RigidBodyComponent::PhysicsMode::Static) {
                    push.color = glm::vec4(0.2f, 1.0f, 0.2f, 1.0f);
                } else {
                    push.color = glm::vec4(0.2f, 0.9f, 1.0f, 1.0f);
                }
            }

            uint32_t vertexCount = kBoxVertexCount;

            switch (collider.shape) {
                case ColliderComponent::ShapeType::Sphere: {
                    const glm::vec3 absScale = glm::abs(transform.scale);
                    float           radius   = collider.radius * glm::max(absScale.x, glm::max(absScale.y, absScale.z));
                    push.shapeType           = static_cast<int>(ShapeType::Sphere);
                    push.shapeParams         = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
                    vertexCount              = kSphereVertexCount;
                    break;
                }
                case ColliderComponent::ShapeType::Capsule: {
                    float radius     = collider.radius * glm::max(glm::abs(transform.scale.x), glm::abs(transform.scale.z));
                    float halfHeight = (collider.size.y * 0.5f) * glm::abs(transform.scale.y);
                    push.shapeType   = static_cast<int>(ShapeType::Capsule);
                    push.shapeParams = glm::vec4(radius, halfHeight, 0.0f, 0.0f);
                    vertexCount      = kCapsuleVertexCount;
                    break;
                }
                case ColliderComponent::ShapeType::Mesh: {
                    glm::vec3 halfExtents = glm::max(transform.scale * 0.5f, glm::vec3(0.05f));
                    if (registry.all_of<ModelComponent>(entity)) {
                        const auto& modelComp = registry.get<ModelComponent>(entity);
                        if (modelComp.model) {
                            const AABB& localBounds = modelComp.model->getLocalBounds();
                            if (localBounds.isValid()) {
                                halfExtents = glm::max(glm::abs(localBounds.extents() * transform.scale), glm::vec3(0.05f));
                            }
                        }
                    }
                    push.shapeType   = static_cast<int>(ShapeType::Box);
                    push.shapeParams = glm::vec4(halfExtents, 0.0f);
                    vertexCount      = kBoxVertexCount;
                    break;
                }
                case ColliderComponent::ShapeType::Box:
                default: {
                    glm::vec3 halfExtents = glm::max(glm::abs(collider.size * 0.5f * transform.scale), glm::vec3(0.05f));
                    push.shapeType        = static_cast<int>(ShapeType::Box);
                    push.shapeParams      = glm::vec4(halfExtents, 0.0f);
                    vertexCount           = kBoxVertexCount;
                    break;
                }
            }

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstantData),
                &push);

            vkCmdDraw(frameInfo.commandBuffer, vertexCount, 1, 0, 0);
        }
    }

}  // namespace engine
