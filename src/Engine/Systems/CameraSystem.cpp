#include "Engine/Systems/CameraSystem.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    struct CameraDebugPush {
        glm::mat4 modelMatrix;
        glm::vec4 color;
        float     fovY;
        float     aspectRatio;
        float     nearZ;
        float     farZ;
    };

    CameraSystem::CameraSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device) {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    CameraSystem::~CameraSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void CameraSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange const pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(CameraDebugPush),
        };

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
        };

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create camera debug pipeline layout!");
        }
    }

    void CameraSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass                        = renderPass;
        pipelineConfig.pipelineLayout                    = pipelineLayout;
        pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipeline = std::make_unique<Pipeline>(device, std::string(SHADER_PATH) + "debug_frustum.vert.spv", std::string(SHADER_PATH) + "debug_frustum.frag.spv", pipelineConfig);
    }

    void CameraSystem::render(FrameInfo& frameInfo) const {
        pipeline->bind(frameInfo.commandBuffer);
        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "CameraSystem: global descriptor set is null");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

        auto&       registry    = frameInfo.scene->getRegistry();
        float const aspectRatio = (frameInfo.extent.height > 0) ? static_cast<float>(frameInfo.extent.width) / static_cast<float>(frameInfo.extent.height) : 1.0f;

        auto view = registry.view<CameraComponent, TransformComponent>();
        for (auto entity : view) {
            if (entity == frameInfo.cameraEntity) {
                continue;
            }

            auto [cameraComp, transform] = view.get<CameraComponent, TransformComponent>(entity);

            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix      = glm::translate(modelMatrix, transform.translation);
            modelMatrix      = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix      = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix      = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

            CameraDebugPush push{};
            push.modelMatrix = modelMatrix;
            push.color       = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
            push.fovY        = glm::radians(cameraComp.fovY);
            push.aspectRatio = aspectRatio;
            push.nearZ       = cameraComp.nearZ;
            push.farZ        = cameraComp.farZ;

            vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

            vkCmdDraw(frameInfo.commandBuffer, 24, 1, 0, 0);
        }
    }

    void CameraSystem::update(FrameInfo& frameInfo, float aspectRatio) {
        auto& registry = frameInfo.scene->getRegistry();
        if (registry.valid(frameInfo.cameraEntity)) {
            if (registry.all_of<CameraComponent, TransformComponent>(frameInfo.cameraEntity)) {
                auto&       cameraComp = registry.get<CameraComponent>(frameInfo.cameraEntity);
                const auto& transform  = registry.get<TransformComponent>(frameInfo.cameraEntity);

                updateCamera(cameraComp, transform, aspectRatio);

                frameInfo.camera = cameraComp.camera;
            }
        }
    }

    void CameraSystem::updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio) {
        if (!cameraComp.isOrthographic) {
            cameraComp.camera.setPerspectiveProjection(glm::radians(cameraComp.fovY), aspectRatio, cameraComp.nearZ, cameraComp.farZ);
        } else {
            float const orthoHeight = cameraComp.orthoSize;
            float const orthoWidth  = aspectRatio * orthoHeight;
            cameraComp.camera.setOrtographicProjection(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, cameraComp.nearZ, cameraComp.farZ);
        }

        cameraComp.camera.setViewYXZ(transform.translation, transform.rotation);

        cameraComp.camera.updateFrustum();
    }
}  // namespace engine
