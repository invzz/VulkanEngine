
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "app.hpp"

#include <GLFW/glfw3.h>

#include <chrono>
#include <glm/common.hpp>
#include <glm/glm.hpp>

#include "3dEngine/Camera.hpp"
#include "3dEngine/CameraSystem.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/InputSystem.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Mouse.hpp"
#include "3dEngine/SimpleRenderSystem.hpp"
#include "3dEngine/Window.hpp"
#include "CubeModel.hpp"

namespace engine {
  struct SimplePushConstantData
  {
    glm::mat2 transform{1.0f};
    glm::vec2 offset;
    alignas(16) glm::vec3 color;
  };

  App::App() {}

  App::~App() = default;

  void App::run()
  {
    // Needs to be here to ensure correct construction order
    Camera   camera{};
    Keyboard keyboard{window};
    Mouse    mouse{window};

    // Systems
    InputSystem        inputSystem{keyboard, mouse};
    CameraSystem       cameraSystem{};
    SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass()};

    GameObject cameraObject            = GameObject::create();
    cameraObject.transform.translation = {0.0f, 0.0f, -2.5f};

    auto currentTime = std::chrono::high_resolution_clock::now();
    loadGameObjects();
    // instantiate simple render system

    // Game loop
    while (!window.shouldClose())
    {
      // Poll for window events
      glfwPollEvents();

      auto  newTime   = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
      currentTime     = newTime;
      frameTime       = glm::min(frameTime, 0.1f);

      inputSystem.update(frameTime, cameraObject);
      cameraSystem.updatePerspective(camera, cameraObject, renderer.getAspectRatio());

      if (auto commandBuffer = renderer.beginFrame())
      {
        renderer.beginSwapChainRenderPass(commandBuffer);
        simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
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

    std::shared_ptr<Model> cubeModel = Model::createModelFromFile(device, "/flat_vase.obj");

    GameObject cube = GameObject::create();
    cube.model      = cubeModel;

    cube.transform.translation = {.0f, .0f, 5.f};
    cube.transform.scale       = {3.f, 1.f, 3.f};

    gameObjects.push_back(std::move(cube));
  }

} // namespace engine