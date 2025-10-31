#include "app.hpp"

#include <array>
#include <stdexcept>

#include "Device.hpp"
#include "SierpinskiTriangle.hpp"
#include "Window.hpp"

namespace engine {

    App::App()
    {
        loadModels();
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

    void App::loadModels()
    {
        model = std::make_unique<Model>(
                device,
                SierpinskiTriangle::create(2, {-0.5f, -0.5f}, {0.8f, -0.8f}, {0.0f, 0.8f}));
    }

    void App::createPipeline()
    {
        auto pipelineConfig =
                Pipeline::defaultPipelineConfigInfo(swapChain->width(), swapChain->height());
        pipelineConfig.renderPass     = swapChain->getRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline                      = std::make_unique<Pipeline>(device,
                                              SHADER_PATH "/simple_shader.vert.spv",
                                              SHADER_PATH "/simple_shader.frag.spv",
                                              pipelineConfig);
    }

    void App::createPipelineLayout()
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges    = nullptr,
        };
        if (vkCreatePipelineLayout(device.device(),
                                   &pipelineLayoutInfo,
                                   nullptr,
                                   &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout!");
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
            throw std::runtime_error("failed to allocate command buffers!");
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
            throw std::runtime_error("failed to acquire swap chain image!");
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

        // Create a new swap chain with the updated extent
        swapChain = std::make_unique<SwapChain>(device, extent);

        // Recreate pipeline and command buffers to match the new swap chain
        createPipeline();

        // Free existing command buffers before recreating
        // createCommandBuffers();
    }

    void App::recordCommandBuffer(int imageIndex)
    {
        if (VkCommandBufferBeginInfo beginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    // .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            };
            vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
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

        pipeline->bind(commandBuffers[imageIndex]);

        model->bind(commandBuffers[imageIndex]);
        model->draw(commandBuffers[imageIndex]);

        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

} // namespace engine