
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "app.hpp"

#include <GLFW/glfw3.h>

#include <chrono>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <iostream>

#include "3dEngine/Camera.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Mouse.hpp"
#include "3dEngine/Window.hpp"
#include "3dEngine/ansi_colors.hpp"

// Systems
#include "3dEngine/systems/CameraSystem.hpp"
#include "3dEngine/systems/InputSystem.hpp"
#include "3dEngine/systems/PointLightSystem.hpp"
#include "3dEngine/systems/SimpleRenderSystem.hpp"

// Models
#include "CubeModel.hpp"

namespace engine {

  App::App()
  {
    globalPool = DescriptorPool::Builder(device)
                         .setMaxSets(SwapChain::maxFramesInFlight())
                         .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::maxFramesInFlight())
                         .build();
    loadGameObjects();
  }
  App::~App() = default;
  void App::run()
  {
    GlobalUbo                            ubo{};
    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::maxFramesInFlight());

    for (auto& buffer : uboBuffers)
    {
      buffer = std::make_unique<Buffer>(device,
                                        sizeof(ubo),
                                        1,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                        device.getProperties().limits.minUniformBufferOffsetAlignment);
      buffer->map();
    }

    auto globalSetLayout = DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).build();
    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::maxFramesInFlight());
    for (size_t i = 0; i < globalDescriptorSets.size(); i++)
    {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      DescriptorWriter(*globalSetLayout, *globalPool).writeBuffer(0, &bufferInfo).build(globalDescriptorSets[i]);
    }

    // Needs to be here to ensure correct construction order
    Camera   camera{};
    Keyboard keyboard{window};
    Mouse    mouse{window};

    // Systems
    InputSystem        inputSystem{keyboard, mouse};
    CameraSystem       cameraSystem{};
    SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
    PointLightSystem   pointLightSystem{device, renderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

    GameObject cameraObject            = GameObject::create();
    cameraObject.transform.translation = {0.0f, -0.2f, -2.5f};
    auto currentTime                   = std::chrono::high_resolution_clock::now();

    // FPS counter
    auto   lastFpsTime = currentTime;
    int    frameCount  = 0;
    double lastFps     = 0.0;

    // Game loop
    while (!window.shouldClose())
    {
      glfwPollEvents();

      auto  newTime   = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
      currentTime     = newTime;
      frameTime       = glm::min(frameTime, 0.1f);

      // Update FPS every second
      frameCount++;
      if (auto elapsed = std::chrono::duration<double>(currentTime - lastFpsTime).count(); elapsed >= 1.0)
      {
        lastFps     = frameCount / elapsed;
        frameCount  = 0;
        lastFpsTime = currentTime;
        std::cout << YELLOW << "FPS: " << RESET << static_cast<int>(lastFps) << "\r" << std::flush;
      }

      inputSystem.update(frameTime, cameraObject);
      cameraSystem.updatePerspective(camera, cameraObject, renderer.getAspectRatio());

      if (auto commandBuffer = renderer.beginFrame())
      {
        int frameIndex = renderer.getFrameIndex();

        FrameInfo frameInfo{.frameIndex          = frameIndex,
                            .frameTime           = frameTime,
                            .commandBuffer       = commandBuffer,
                            .camera              = camera,
                            .globalDescriptorSet = globalDescriptorSets[frameIndex],
                            .gameObjects         = gameObjects};

        pointLightSystem.update(frameInfo, ubo);

        // update uniform buffer object
        ubo.projection = camera.getProjection();
        ubo.view       = camera.getView();
        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        // render the scene
        renderer.beginSwapChainRenderPass(commandBuffer);
        simpleRenderSystem.render(frameInfo);
        pointLightSystem.render(frameInfo);
        renderer.endSwapChainRenderPass(commandBuffer);
        renderer.endFrame();
      }
    }

    device.WaitIdle();
  }

  void App::loadGameObjects()
  {
    if (!gameObjects.empty())
    {
      return;
    }

    std::vector<glm::vec3> lightColors{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, 1.f, 0.f}

    };

    for (size_t i = 0; i < lightColors.size(); i++)
    {
      auto pointLight = GameObject::makePointLightObject(0.2f, lightColors[i], 0.2f);

      float angle  = (2.0f * glm::pi<float>() * static_cast<float>(i)) / static_cast<float>(lightColors.size());
      float radius = 3.0f;

      pointLight.transform.translation = {radius * std::cos(angle), -1.f, radius * std::sin(angle)};

      gameObjects.try_emplace(pointLight.getId(), std::move(pointLight));
    }

    std::vector<std::shared_ptr<Model>> models{
            Model::createModelFromFile(device, "/flat_vase.obj"),
            Model::createModelFromFile(device, "/smooth_vase.obj"),
    };

    const int   instancesPerModel = 4;
    const float spacing           = 1.0f;
    const int   totalInstances    = static_cast<int>(models.size()) * instancesPerModel;
    const float origin            = 0.5f * static_cast<float>(totalInstances - 1);

    auto floor                  = GameObject::create();
    floor.model                 = Model::createModelFromFile(device, "/quad.obj");
    floor.transform.scale       = {4.0f, 1.f, 4.0f};
    floor.transform.translation = {0.0f, 0.0f, 0.0f};
    gameObjects.try_emplace(floor.getId(), std::move(floor));

    int instanceIndex = 0;
    for (const auto& model : models)
    {
      for (int copy = 0; copy < instancesPerModel; ++copy)
      {
        GameObject vase = GameObject::create();
        vase.model      = model;

        vase.transform.translation = {
                (static_cast<float>(instanceIndex) - origin) * spacing,
                0.0f,
                0.0f,
        };

        vase.transform.scale = {1.f, 1.f, 1.f};

        gameObjects.try_emplace(vase.getId(), std::move(vase));

        ++instanceIndex;
      }
    }
  }

} // namespace engine