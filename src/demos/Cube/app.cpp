
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
#include "3dEngine/systems/ObjectSelectionSystem.hpp"
#include "3dEngine/systems/PBRRenderSystem.hpp"
#include "3dEngine/systems/PointLightSystem.hpp"

// Demo specific
#include "RenderContext.hpp"
#include "SceneLoader.hpp"

namespace engine {

  App::App()
  {
    SceneLoader::loadScene(device, gameObjects);
  }
  App::~App() = default;

  void App::run()
  {
    // Setup rendering context (descriptors, UBO buffers)
    RenderContext renderContext{device};

    // Setup scene objects (camera, input devices)
    Camera   camera{};
    Keyboard keyboard{window};
    Mouse    mouse{window};

    GameObject cameraObject            = GameObject::create();
    cameraObject.transform.translation = {0.0f, -0.2f, -2.5f};

    // Initialize systems
    ObjectSelectionSystem objectSelectionSystem{keyboard};
    InputSystem           inputSystem{keyboard, mouse};
    CameraSystem          cameraSystem{};
    PBRRenderSystem       pbrRenderSystem{device, renderer.getSwapChainRenderPass(), renderContext.getGlobalSetLayout()};
    PointLightSystem      pointLightSystem{device, renderer.getSwapChainRenderPass(), renderContext.getGlobalSetLayout()};

    // Selection state (persisted across frames)
    GameObject::id_t selectedObjectId = 0;
    GameObject*      selectedObject   = nullptr;

    // Timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto lastFpsTime = currentTime;
    int  frameCount  = 0;

    // Main game loop
    while (!window.shouldClose())
    {
      glfwPollEvents();

      // Calculate frame time
      auto  newTime   = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
      currentTime     = newTime;
      frameTime       = glm::min(frameTime, 0.1f);

      // FPS display
      frameCount++;
      if (auto elapsed = std::chrono::duration<double>(currentTime - lastFpsTime).count(); elapsed >= 1.0)
      {
        std::cout << "[ " << YELLOW << "FPS " << RESET << " ]" << static_cast<int>(frameCount / elapsed) << "\r" << std::flush;
        frameCount  = 0;
        lastFpsTime = currentTime;
      }

      // Begin frame
      if (auto commandBuffer = renderer.beginFrame())
      {
        int frameIndex = renderer.getFrameIndex();

        // Build frame info
        FrameInfo frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = camera,
                .globalDescriptorSet = renderContext.getGlobalDescriptorSet(frameIndex),
                .gameObjects         = gameObjects,
                .selectedObjectId    = selectedObjectId,
                .selectedObject      = selectedObject,
        };

        // Update systems
        objectSelectionSystem.update(frameInfo);
        inputSystem.update(frameInfo, cameraObject);
        cameraSystem.update(frameInfo, cameraObject, renderer.getAspectRatio());

        // Persist selection state
        selectedObjectId = frameInfo.selectedObjectId;
        selectedObject   = frameInfo.selectedObject;

        // Update UBO
        GlobalUbo ubo{};
        pointLightSystem.update(frameInfo, ubo);
        ubo.projection     = camera.getProjection();
        ubo.view           = camera.getView();
        ubo.cameraPosition = glm::vec4(cameraObject.transform.translation, 1.0f);
        renderContext.updateUBO(frameIndex, ubo);

        // Render
        renderer.beginSwapChainRenderPass(commandBuffer);
        pbrRenderSystem.render(frameInfo);
        pointLightSystem.render(frameInfo);
        renderer.endSwapChainRenderPass(commandBuffer);
        renderer.endFrame();
      }
    }

    device.WaitIdle();
  }

} // namespace engine