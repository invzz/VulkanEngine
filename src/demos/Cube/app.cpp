
#include "3dEngine/Exceptions.hpp"

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
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/SimpleRenderSystem.hpp"
#include "3dEngine/Window.hpp"
#include "CubeModel.hpp"
#include "app.hpp"

namespace engine {
    static constexpr float frameDt = 1.f / 120.f;
    struct SimplePushConstantData
    {
        glm::mat2 transform{1.0f};
        glm::vec2 offset;
        alignas(16) glm::vec3 color;
    };

    App::App()
    {
        loadGameObjects();
    }

    App::~App() = default;

    void setUpPerspectiveCamera(Camera& camera, float aspectRatio)
    {
        camera.setPerspectiveProjection(glm::radians(45.f), aspectRatio, 0.1f, 10.f);
    }

    void setUpOrthographicCamera(Camera& camera, float aspectRatio)
    {
        float orthoHeight = 1.5f;
        float orthoWidth  = orthoHeight * aspectRatio;
        camera.setOrtographicProjection(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, -10.f, 10.f);
    }

    void rotateObjectOverTime(GameObject& obj, float deltaTime)
    {
        obj.transform.rotation.y += glm::radians(90.f) * deltaTime;
        if (obj.transform.rotation.y > glm::two_pi<float>())
        {
            obj.transform.rotation.y -= glm::two_pi<float>();
        }
    }

    void App::run()
    {
        // create a camera
        Camera camera{};
        camera.setViewTarget(glm::vec3(-2.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 0.5f), glm::vec3(0.f, -1.f, 0.f));

        // instantiate simple render system
        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass()};

        while (!window.shouldClose())
        {
            // get the time at the start of the frame
            static float accumulatedTime = 0.f;
            static float previousTime    = static_cast<float>(glfwGetTime());
            float        currentTime     = static_cast<float>(glfwGetTime());
            float        deltaTime       = currentTime - previousTime;
            previousTime                 = currentTime;
            accumulatedTime += deltaTime;
            // Poll for window events
            glfwPollEvents();

            float aspectRatio = renderer.getAspectRatio();

            setUpPerspectiveCamera(camera, aspectRatio);

            // make the camera orbit around the origin
            float radius    = 2.5f;
            float camX      = sinf(currentTime) * radius;
            float camZ      = cosf(currentTime) * radius;
            auto  camPos    = glm::vec3{camX, -2.f, camZ};
            auto  targetPos = glm::vec3{0.f, 0.f, 0.5f};

            // fixed timestep update loop
            while (accumulatedTime >= frameDt)
            {
                // update game logic here with fixed timestep (frameDt)
                accumulatedTime -= frameDt;
                camera.setViewTarget(camPos, targetPos, glm::vec3{0.f, -1.f, 0.f});
                rotateObjectOverTime(gameObjects[0], frameDt);
            }
            if (auto commandBuffer = renderer.beginFrame())
            {
                renderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
                renderer.endSwapChainRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }

        // Wait for the device to finish operations before exiting
        vkDeviceWaitIdle(device.device());
    }

    void App::loadGameObjects()
    {
        if (!gameObjects.empty())
        {
            return;
        }

        std::shared_ptr<Model> cubeModel = createCubeModel(device, {0.0f, 0.0f, 0.0f});

        GameObject cube = GameObject::createGameObjectWithId();
        cube.model      = cubeModel;

        cube.transform.translation = {.0f, .0f, .5f};
        cube.transform.scale       = {.5f, .5f, .5f};

        gameObjects.push_back(std::move(cube));
    }

} // namespace engine