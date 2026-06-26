#include "Engine/Systems/LightSystem.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {
    namespace {
        void updateTargetLockedLights(entt::registry& registry) {
            {
                auto dirView = registry.view<DirectionalLightComponent, TransformComponent>();
                for (auto entity : dirView) {
                    auto& dirLight  = dirView.get<DirectionalLightComponent>(entity);
                    auto& transform = dirView.get<TransformComponent>(entity);
                    if (dirLight.useTargetPoint) {
                        transform.lookAt(dirLight.targetPoint);
                    }
                }
            }

            {
                auto spotView = registry.view<SpotLightComponent, TransformComponent>();
                for (auto entity : spotView) {
                    auto& spotLight = spotView.get<SpotLightComponent>(entity);
                    auto& transform = spotView.get<TransformComponent>(entity);
                    if (spotLight.useTargetPoint) {
                        transform.lookAt(spotLight.targetPoint);
                    }
                }
            }
        }
    }  // namespace

    struct PointLightPushConstants {
        /* data */
        glm::vec4 position{};  // w component unused
        glm::vec4 color{};     // w component is intensity
        float     radius{};
    };

    LightSystem::LightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device) {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
        createDirectionalLightPipelineLayout(globalSetLayout);
        createDirectionalLightPipeline(renderPass);
        createSpotLightPipelineLayout(globalSetLayout);
        createSpotLightPipeline(renderPass);
    }

    void LightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(PointLightPushConstants),
        };

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
        };
        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create pipeline layout!");
        }
    }
    LightSystem::~LightSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
        vkDestroyPipelineLayout(device.device(), directionalPipelineLayout, nullptr);
        vkDestroyPipelineLayout(device.device(), spotPipelineLayout, nullptr);
    }

    void LightSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipeline                                         = std::make_unique<Pipeline>(device, std::string(SHADER_PATH) + R"(point_light.vert.spv)", std::string(SHADER_PATH) + R"(point_light.frag.spv)", pipelineConfig);
    }

    void LightSystem::render(FrameInfo& frameInfo) {
        pipeline->bind(frameInfo.commandBuffer);

        // Defensive: validate descriptor set before binding
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "LightSystem: global descriptor set is null");

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

        constexpr float kPointLightDebugRadiusScale = 0.15f;

        auto view = frameInfo.scene->getRegistry().view<PointLightComponent, TransformComponent>();
        for (auto entity : view) {
            auto [pointLight, transform] = view.get<PointLightComponent, TransformComponent>(entity);

            PointLightPushConstants push{};
            push.position = glm::vec4(transform.translation, 1.f);
            push.color    = glm::vec4(pointLight.color, pointLight.intensity);
            push.radius   = transform.scale.x * kPointLightDebugRadiusScale;

            vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PointLightPushConstants), &push);
            // inefficient to draw a quad for each light, but okay for demo purposes
            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }

        // Render directional lights as arrows
        directionalPipeline->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "LightSystem: global descriptor set is null for directional lights");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, directionalPipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

        auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
        for (auto entity : dirView) {
            auto [dirLight, transform] = dirView.get<DirectionalLightComponent, TransformComponent>(entity);

            // Create a model matrix that orients the arrow in the light direction
            auto modelMatrix = glm::mat4(1.0f);

            // Apply rotation to orient arrow at the origin.
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

            struct DirectionalLightPush {
                glm::mat4 modelMatrix;
                glm::vec4 color;
            } push;

            push.modelMatrix = modelMatrix;
            push.color       = glm::vec4(dirLight.color, dirLight.intensity);

            vkCmdPushConstants(frameInfo.commandBuffer, directionalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

            vkCmdDraw(frameInfo.commandBuffer, 18, 1, 0, 0);  // 18 vertices for arrow
        }

        // Render spot lights as cones
        spotPipeline->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "LightSystem: global descriptor set is null for spot lights");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotPipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

        auto spotView = frameInfo.scene->getRegistry().view<SpotLightComponent, TransformComponent>();
        for (auto entity : spotView) {
            auto [spotLight, transform] = spotView.get<SpotLightComponent, TransformComponent>(entity);

            // Create a model matrix that positions and orients the cone
            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix      = glm::translate(modelMatrix, transform.translation);

            // Apply rotation to orient cone
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

            struct SpotLightPush {
                glm::mat4 modelMatrix;
                glm::vec4 color;
                float     coneAngle;
            } push;

            push.modelMatrix = modelMatrix;
            push.color       = glm::vec4(spotLight.color, spotLight.intensity);
            push.coneAngle   = glm::radians(spotLight.outerCutoffAngle);

            vkCmdPushConstants(frameInfo.commandBuffer, spotPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

            // Draw cone: 32 segments * 3 vertices per triangle = 96 vertices
            vkCmdDraw(frameInfo.commandBuffer, 96, 1, 0, 0);
        }
    }

    void LightSystem::updateTargetLockedLight(entt::entity entity, Scene* scene) {
        auto& registry = scene->getRegistry();

        // Update directional light target tracking
        if (registry.all_of<DirectionalLightComponent>(entity)) {
            auto& dirLight = registry.get<DirectionalLightComponent>(entity);
            if (dirLight.useTargetPoint) {
                registry.get<TransformComponent>(entity).lookAt(dirLight.targetPoint);
            }
        }

        // Update spot light target tracking
        if (registry.all_of<SpotLightComponent>(entity)) {
            auto& spotLight = registry.get<SpotLightComponent>(entity);
            if (spotLight.useTargetPoint) {
                registry.get<TransformComponent>(entity).lookAt(spotLight.targetPoint);
            }
        }
    }

    void LightSystem::updateAllTargetLockedLights(Scene& scene) {
        updateTargetLockedLights(scene.getRegistry());
    }

    void LightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) const {
        ubo.pointLightCount       = 0;
        ubo.directionalLightCount = 0;
        ubo.spotLightCount        = 0;

        auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, glm::vec3(0.f, -1.f, 0.f));

        auto& registry = frameInfo.scene->getRegistry();

        // Keep target-locked lights oriented correctly before any other logic.
        updateTargetLockedLights(registry);

        // Process point lights
        auto pointView = registry.view<TransformComponent, PointLightComponent>();
        for ([[maybe_unused]] auto entity : pointView) {
            ubo.pointLightCount++;
        }

        // Process directional lights — engine supports at most one directional light for shading.
        auto dirView = registry.view<TransformComponent, DirectionalLightComponent>();
        for (auto entity : dirView) {
            // Count only the first directional light found
            ubo.directionalLightCount = 1;
            break;
        }

        // Process spot lights
        auto spotView = registry.view<TransformComponent, SpotLightComponent>();
        for (auto entity : spotView) {
            auto [transform, spotLight] = spotView.get<TransformComponent, SpotLightComponent>(entity);
            ubo.spotLightCount++;
        }
    }

    void LightSystem::createDirectionalLightPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(glm::mat4) + sizeof(glm::vec4),  // modelMatrix + color
        };

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
        };

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &directionalPipelineLayout) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create directional light pipeline layout!");
        }
    }

    void LightSystem::createDirectionalLightPipeline(VkRenderPass renderPass) {
        assert(directionalPipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = directionalPipelineLayout;
        pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        directionalPipeline                              = std::make_unique<Pipeline>(device, std::string(SHADER_PATH) + R"(directional_light.vert.spv)", std::string(SHADER_PATH) + R"(directional_light.frag.spv)", pipelineConfig);
    }

    void LightSystem::createSpotLightPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(float),  // modelMatrix + color + coneAngle
        };

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
        };

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &spotPipelineLayout) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create spot light pipeline layout!");
        }
    }

    void LightSystem::createSpotLightPipeline(VkRenderPass renderPass) {
        assert(spotPipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = spotPipelineLayout;
        pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;

        // Enable alpha blending for semi-transparent cone
        pipelineConfig.colorBlendAttachment.blendEnable         = VK_TRUE;
        pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineConfig.colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pipelineConfig.colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

        spotPipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/spot_light.vert.spv", SHADER_PATH "/spot_light.frag.spv", pipelineConfig);
    }
}  // namespace engine
