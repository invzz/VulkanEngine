
#include "Exceptions.hpp"

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
#include <unordered_map>

#include "Device.hpp"
#include "SierpinskiTriangle.hpp"
#include "Window.hpp"
#include "app.hpp"

namespace engine {

    struct SimplePushConstantData
    {
        glm::mat2 transform{1.0f};
        glm::vec2 offset;
        alignas(16) glm::vec3 color;
    };

    App::App()
    {
        loadGameObjects();
        createPipelineLayout();
        createPipeline();
    }

    App::~App()
    {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void App::run()
    {
        while (!window.shouldClose())
        {
            // Poll for window events
            glfwPollEvents();
            // Render a frame
            if (auto commandBuffer = renderer.beginFrame())
            {
                // TODO //
                // begin offscreen shadow pass
                // render shadow casters
                // end offscreen shadow pass

                renderer.beginSwapChainRenderPass(commandBuffer);
                renderGameObjects(commandBuffer);
                renderer.endSwapChainRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }
        // Wait for the device to finish operations before exiting
        vkDeviceWaitIdle(device.device());
    }

    void App::loadGameObjects()
    {
        auto model = std::make_unique<Model>(
                device,
                SierpinskiTriangle::create(5, {-0.5f, 0.5f}, {0.0f, -0.5f}, {0.5f, 0.5f}));

        auto      transform2d = Transform2DComponent{.translation = {-0.5f, -0.5f},
                                                     .scale       = {0.4f, 0.4f},
                                                     .rotation    = glm::radians(0.0f)};
        glm::vec3 color       = {1.0f, 0.0f, 0.0f};

        GameObject triangle1  = GameObject::createGameObjectWithId();
        triangle1.model       = std::move(model);
        triangle1.transform2d = transform2d;
        triangle1.color       = color;

        gameObjects.push_back(std::move(triangle1));
    }

    void App::createPipeline()
    {
        assert(pipelineLayout != VK_NULL_HANDLE &&
               "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);

        pipelineConfig.renderPass     = renderer.getSwapChainRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline                      = std::make_unique<Pipeline>(device,
                                              SHADER_PATH "/simple_shader.vert.spv",
                                              SHADER_PATH "/simple_shader.frag.spv",
                                              pipelineConfig);
    }

    void App::createPipelineLayout()
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

    void App::renderGameObjects(VkCommandBuffer commandBuffer)
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