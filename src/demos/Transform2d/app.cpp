#include "app.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>
#include <unordered_map>

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
        gameObjects.clear();
        animationTracks.clear();

        struct LayerConfig
        {
            int       order;
            float     radius;
            float     radialPulse;
            float     radialFrequency;
            glm::vec2 baseScale;
            glm::vec2 scalePulse;
            float     scaleFrequency;
            float     orbitSpeed;
            float     phaseOffset;
            float     baseRotation;
            float     rotationSpeed;
            glm::vec3 baseColor;
            glm::vec3 colorCycle;
            float     colorFrequency;
        };

        // Increase arms and layers to create a dense kaleidoscope. Keep models cached by order.
        const int armCount                      = 200; // many more repeated arms around the circle
        const std::array<LayerConfig, 5> layers = {
                LayerConfig{6,
                            0.78f,
                            0.06f,
                            0.6f,
                            {0.28f, 0.24f},
                            {0.06f, 0.05f},
                            1.2f,
                            0.28f,
                            0.0f,
                            0.0f,
                            0.9f,
                            {0.9f, 0.5f, 0.7f},
                            {0.18f, 0.28f, 0.36f},
                            0.6f},
                LayerConfig{5,
                            0.6f,
                            0.05f,
                            0.8f,
                            {0.24f, 0.2f},
                            {0.05f, 0.04f},
                            1.0f,
                            -0.45f,
                            glm::quarter_pi<float>() * 0.25f,
                            glm::quarter_pi<float>() * 0.4f,
                            1.15f,
                            {0.4f, 0.9f, 0.76f},
                            {0.12f, 0.22f, 0.18f},
                            0.9f},
                LayerConfig{4,
                            0.44f,
                            0.04f,
                            1.0f,
                            {0.22f, 0.18f},
                            {0.05f, 0.035f},
                            1.25f,
                            0.6f,
                            glm::quarter_pi<float>() * 0.5f,
                            glm::quarter_pi<float>() * -0.2f,
                            1.35f,
                            {0.35f, 0.7f, 0.95f},
                            {0.12f, 0.18f, 0.22f},
                            1.3f},
                LayerConfig{3,
                            0.28f,
                            0.03f,
                            1.6f,
                            {0.18f, 0.15f},
                            {0.04f, 0.03f},
                            1.6f,
                            -0.9f,
                            glm::half_pi<float>() * 0.25f,
                            glm::quarter_pi<float>() * 0.1f,
                            1.75f,
                            {1.0f, 0.78f, 0.35f},
                            {0.14f, 0.12f, 0.28f},
                            1.7f},
                LayerConfig{2,
                            0.12f,
                            0.02f,
                            2.2f,
                            {0.14f, 0.12f},
                            {0.03f, 0.02f},
                            2.0f,
                            1.2f,
                            glm::quarter_pi<float>() * 0.75f,
                            glm::quarter_pi<float>() * -0.45f,
                            2.0f,
                            {1.0f, 0.95f, 0.88f},
                            {0.08f, 0.06f, 0.05f},
                            2.1f},
        };

        std::unordered_map<int, std::shared_ptr<Model>> modelCache;

        auto acquireModel = [&](int order) {
            auto& cachedModel = modelCache[order];
            if (cachedModel == nullptr)
            {
                cachedModel = std::make_shared<Model>(device,
                                                      SierpinskiTriangle::create(order,
                                                                                 {-0.5f, 0.5f},
                                                                                 {0.0f, -0.5f},
                                                                                 {0.5f, 0.5f}));
            }
            return cachedModel;
        };

        for (const auto& layer : layers)
        {
            for (int arm = 0; arm < armCount; ++arm)
            {
                const float    armFraction = static_cast<float>(arm) / static_cast<float>(armCount);
                AnimationTrack track{};
                track.armIndex             = arm;
                track.armCount             = armCount;
                track.radius               = layer.radius;
                track.orbitSpeed           = layer.orbitSpeed;
                track.orbitPhase           = layer.phaseOffset + glm::two_pi<float>() * armFraction;
                track.radialPulseAmplitude = layer.radialPulse;
                track.radialPulseFrequency = layer.radialFrequency + 0.15f * armFraction;
                track.baseScale            = layer.baseScale;
                track.scalePulseAmplitude  = layer.scalePulse;
                track.scalePulseFrequency  = layer.scaleFrequency + 0.1f * armFraction;
                track.baseRotation         = layer.baseRotation;
                track.rotationSpeed        = layer.rotationSpeed * ((arm % 2 == 0) ? 1.0f : -1.0f);
                track.mirror               = (arm % 2) == 1;
                track.baseColor            = layer.baseColor;
                track.colorCycle           = layer.colorCycle;
                track.colorFrequency       = layer.colorFrequency + 0.2f * armFraction;

                auto triangle                    = GameObject::createGameObjectWithId();
                triangle.model                   = acquireModel(0);
                triangle.color                   = track.baseColor;
                triangle.transform2d.translation = {0.0f, 0.0f};
                triangle.transform2d.scale       = track.baseScale;
                if (track.mirror)
                {
                    triangle.transform2d.scale.x *= -1.0f;
                }
                triangle.transform2d.rotation = track.baseRotation;

                gameObjects.push_back(std::move(triangle));
                animationTracks.push_back(track);
            }
        }

        AnimationTrack centerTrack{};
        centerTrack.armIndex             = 0;
        centerTrack.armCount             = 1;
        centerTrack.radius               = 0.0f;
        centerTrack.orbitSpeed           = 0.0f;
        centerTrack.orbitPhase           = 0.0f;
        centerTrack.radialPulseAmplitude = 0.05f;
        centerTrack.radialPulseFrequency = 2.0f;
        centerTrack.baseScale            = {0.32f, 0.32f};
        centerTrack.scalePulseAmplitude  = {0.12f, 0.1f};
        centerTrack.scalePulseFrequency  = 1.8f;
        centerTrack.baseRotation         = 0.0f;
        centerTrack.rotationSpeed        = 2.4f;
        centerTrack.mirror               = false;
        centerTrack.baseColor            = {0.95f, 0.95f, 0.95f};
        centerTrack.colorCycle           = {0.25f, 0.35f, 0.45f};
        centerTrack.colorFrequency       = 3.2f;

        auto center                    = GameObject::createGameObjectWithId();
        center.model                   = acquireModel(0);
        center.color                   = centerTrack.baseColor;
        center.transform2d.translation = {0.0f, 0.0f};
        center.transform2d.scale       = centerTrack.baseScale;
        center.transform2d.rotation    = centerTrack.baseRotation;

        gameObjects.push_back(std::move(center));
        animationTracks.push_back(centerTrack);
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
        const auto timeSeconds = static_cast<float>(glfwGetTime());
        updateAnimation(timeSeconds);

        if (VkCommandBufferBeginInfo beginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    // .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            };
            vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
        {
            throw engine::RuntimeException("failed to begin recording command buffer!");
        }

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color                    = {0.0f, 0.0f, 0.0f, 1.0f};
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

    void App::updateAnimation(float timeSeconds)
    {
        for (size_t i = 0; i < gameObjects.size() && i < animationTracks.size(); ++i)
        {
            auto&       object = gameObjects[i];
            const auto& track  = animationTracks[i];

            const float baseAngle =
                    (track.armCount > 0)
                            ? glm::two_pi<float>() * (static_cast<float>(track.armIndex) /
                                                      static_cast<float>(track.armCount))
                            : 0.0f;
            const float orbitAngle = baseAngle + timeSeconds * track.orbitSpeed + track.orbitPhase;
            const float radialWave =
                    track.radialPulseAmplitude *
                    std::sin(timeSeconds * track.radialPulseFrequency + track.orbitPhase);
            const float radius = track.radius + radialWave;

            glm::vec2 direction{std::cos(orbitAngle), std::sin(orbitAngle)};
            object.transform2d.translation = direction * radius;

            const float scaleWave =
                    0.5f *
                    (std::sin(timeSeconds * track.scalePulseFrequency + track.orbitPhase) + 1.0f);
            glm::vec2 scale = track.baseScale + track.scalePulseAmplitude * scaleWave;
            if (track.mirror)
            {
                scale.x *= -1.0f;
            }
            object.transform2d.scale = scale;

            const float mirrorOffset    = track.mirror ? glm::pi<float>() : 0.0f;
            const float orbitalDrift    = orbitAngle * 0.5f;
            object.transform2d.rotation = track.baseRotation + mirrorOffset + orbitalDrift +
                                          timeSeconds * track.rotationSpeed;

            const float colorWave =
                    0.5f * (std::sin(timeSeconds * track.colorFrequency + track.orbitPhase) + 1.0f);
            glm::vec3 color = track.baseColor + track.colorCycle * colorWave;
            object.color    = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
        }
    }

} // namespace engine