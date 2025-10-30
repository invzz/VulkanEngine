#include "app.hpp"

#include <array>
#include <stdexcept>

#include "Device.hpp"
#include "Window.hpp"

namespace engine {

    App::App()
    {
        createPipelineLayout();
        createPipeline();
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
            // Main loop logic here
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device.device());
    }

    void App::createPipeline()
    {
        auto pipelineConfig =
                Pipeline::defaultPipelineConfigInfo(swapChain.width(), swapChain.height());
        pipelineConfig.renderPass     = swapChain.getRenderPass();
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
        commandBuffers.resize(swapChain.imageCount());

        VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = device.getCommandPool(),
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        };

        if (vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < commandBuffers.size(); i++)
        {
            VkCommandBufferBeginInfo beginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    // .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            };

            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color                    = {0.01f, 0.01f, 0.01f, 1.0f};
            clearValues[1].depthStencil             = {1.0f, 0};

            VkRenderPassBeginInfo renderPassInfo{
                    .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .renderPass  = swapChain.getRenderPass(),
                    .framebuffer = swapChain.getFrameBuffer(static_cast<int>(i)),
                    .renderArea =
                            {
                                    .offset = {0, 0},
                                    .extent = swapChain.getSwapChainExtent(),
                            },
                    .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                    .pClearValues    = clearValues.data(),
            };

            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            pipeline->bind(commandBuffers[i]);

            // 3 vertices, 1 instance, first vertex 0, first instance 0
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffers[i]);
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void App::drawFrame()
    {
        uint32_t imageIndex;
        auto     result = swapChain.acquireNextImage(&imageIndex);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        // submit command buffer and handle synchronization
        result = swapChain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
    }

} // namespace engine