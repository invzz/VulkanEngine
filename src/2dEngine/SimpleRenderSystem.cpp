#include "2dEngine/SimpleRenderSystem.hpp"

#include "2dEngine/Exceptions.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

namespace engine {

    struct SimplePushConstantData
    {
        glm::mat2 transform{1.0f};
        glm::vec2 offset;
        alignas(16) glm::vec3 color;
    };

    SimpleRenderSystem::SimpleRenderSystem(Device& device, VkRenderPass renderPass) : device(device)
    {
        createPipelineLayout();
        createPipeline(renderPass);
    }

    void SimpleRenderSystem::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset     = 0,
                .size       = sizeof(SimplePushConstantData),
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges    = &pushConstantRange,
        };
        if (vkCreatePipelineLayout(device.device(),
                                   &pipelineLayoutInfo,
                                   nullptr,
                                   &pipelineLayout) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to create pipeline layout!");
        }
    }
    SimpleRenderSystem::~SimpleRenderSystem()
    {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
    {
        assert(pipelineLayout != VK_NULL_HANDLE &&
               "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);

        pipelineConfig.renderPass     = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline                      = std::make_unique<Pipeline>(device,
                                              SHADER_PATH "/simple_shader.vert.spv",
                                              SHADER_PATH "/simple_shader.frag.spv",
                                              pipelineConfig);
    }

    void SimpleRenderSystem::renderGameObjects(VkCommandBuffer                commandBuffer,
                                               const std::vector<GameObject>& gameObjects)
    {
        pipeline->bind(commandBuffer);

        for (const auto& gameObject : gameObjects)
        {
            SimplePushConstantData push{};
            push.offset    = gameObject.transform2d.translation;
            push.color     = gameObject.color;
            push.transform = gameObject.transform2d.mat2();

            vkCmdPushConstants(commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(SimplePushConstantData),
                               &push);
            gameObject.model->bind(commandBuffer);
            gameObject.model->draw(commandBuffer);
        }
    }
} // namespace engine
