#include "app.hpp"

#include <array>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

#include "Exceptions.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Device.hpp"
#include "SierpinskiTriangle.hpp"
#include "Window.hpp"

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
        // Initial swap chain creation and related pipeline and command buffer setup
        recreateSwapChain();
        createCommandBuffers();
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
            drawFrame();
        }
        // Wait for the device to finish operations before exiting
        vkDeviceWaitIdle(device.device());
    }

    void App::loadGameObjects()
    {
        auto model = std::make_shared<Model>(
                device,
                SierpinskiTriangle::create(3, {-0.5f, 0.5f}, {0.0f, -0.5f}, {0.5f, 0.5f}));

        auto triangle = GameObject::createGameObjectWithId();

        triangle.model                     = model;
        triangle.color                     = {1.0f, 1.0f, 0.0f};
        triangle.transform2d.translation.x = 0.2f;
        triangle.transform2d.translation.y = 0.0f;
        triangle.transform2d.rotation      = .25f * glm::two_pi<float>();
        triangle.transform2d.scale         = {0.8f, 0.2f};

        gameObjects.push_back(std::move(triangle));
    }

    void App::createPipeline()
    {
        assert(swapChain != nullptr && "Swap chain must be created before pipeline.");
        assert(pipelineLayout != VK_NULL_HANDLE &&
               "Pipeline layout must be created before pipeline.");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);

        pipelineConfig.renderPass     = swapChain->getRenderPass();
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

    void App::createCommandBuffers()
    {
        commandBuffers.resize(swapChain->imageCount());

        if (VkCommandBufferAllocateInfo allocInfo{
                    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool        = device.getCommandPool(),
                    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
            };
            vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) !=
            VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to allocate command buffers!");
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

    void App::drawFrame()
    {
        uint32_t imageIndex;

        auto result = swapChain->acquireNextImage(&imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw engine::RuntimeException("failed to acquire swap chain image!");
        }

        recordCommandBuffer(imageIndex);

        // submit command buffer and handle synchronization
        result = swapChain->submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            window.wasWindowResized())
        {
            window.resetWindowResizedFlag();
            recreateSwapChain();
            return;
        }
    }

    void App::freeCommandBuffers()
    {
        vkFreeCommandBuffers(device.device(),
                             device.getCommandPool(),
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());
        commandBuffers.clear();
    }

    void App::recreateSwapChain()
    {
        VkExtent2D extent = window.getExtent();
        while (extent.width == 0 || extent.height == 0)
        {
            extent = window.getExtent();
            glfwWaitEvents();
        }

        // Wait for the device to be idle before recreating swap chain
        vkDeviceWaitIdle(device.device());

        if (swapChain != nullptr)
        {
            // Pass the old swap chain to the new one for efficient resource reuse
            swapChain = std::make_unique<SwapChain>(device, extent, std::move(swapChain));
            if (swapChain->imageCount() != commandBuffers.size())
            {
                freeCommandBuffers();
                createCommandBuffers();
            }
        }
        else
        {
            swapChain = std::make_unique<SwapChain>(device, extent);
        }

        // Recreate pipeline and command buffers to match the new swap chain
        createPipeline();
    }

    void App::recordCommandBuffer(int imageIndex)
    {
        static int frame = 0;

        frame = (frame + 1) % 1000;

        if (VkCommandBufferBeginInfo beginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    // .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            };
            vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to begin recording command buffer!");
        }

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color                    = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil             = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{
                .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass  = swapChain->getRenderPass(),
                .framebuffer = swapChain->getFrameBuffer(imageIndex),
                .renderArea =
                        {
                                .offset = {0, 0},
                                .extent = swapChain->getSwapChainExtent(),
                        },
                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                .pClearValues    = clearValues.data(),
        };
        vkCmdBeginRenderPass(commandBuffers[imageIndex],
                             &renderPassInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<float>(swapChain->getSwapChainExtent().width),
                .height   = static_cast<float>(swapChain->getSwapChainExtent().height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };

        VkRect2D scissor{
                .offset = {0, 0},
                .extent = swapChain->getSwapChainExtent(),
        };

        vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

        renderGameObjects(commandBuffers[imageIndex]);

        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to record command buffer!");
        }
    }

} // namespace engine