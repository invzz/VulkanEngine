
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
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "CollisionSystem.hpp"
#include "Device.hpp"
#include "GameObject.hpp"
#include "GravityPhysicsSystem.hpp"
#include "Model.hpp"
#include "SimpleModels.hpp"
#include "SimpleRenderSystem.hpp"
#include "SimulationConfig.hpp"
#include "ThreeBodySimulationPhysics.hpp"
#include "TrailSystem.hpp"
#include "Vec2FieldSystem.hpp"
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
    }

    App::~App() = default;

    void App::run()
    {
        // create some models
        std::shared_ptr<Model> squareModel = SimpleModels::createSquareModel(
                device,
                {.5f, .0f}); // offset model by .5 so rotation occurs
                             // at edge rather than center of square
        std::shared_ptr<Model> circleModel = SimpleModels::createCircleModel(device, 64);
        std::shared_ptr<Model> trailQuadModel =
                SimpleModels::createSquareModel(device, {0.0f, 0.0f});

        const float          gravityStrength = SimulationConfig::Physics::gravityStrength;
        GravityPhysicsSystem gravitySystem{gravityStrength};

        // create physics objects
        std::vector<GameObject> physicsObjects{};
        std::vector<glm::vec3>  baseColors{};
        constexpr std::size_t   bodyCount   = SimulationConfig::InitialBodies::count;
        const float             orbitRadius = SimulationConfig::InitialBodies::orbitRadius;

        std::array<glm::vec2, bodyCount> basePositions{};
        for (std::size_t i = 0; i < bodyCount; ++i)
        {
            const float angle =
                    static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(bodyCount);
            basePositions[i] = orbitRadius * glm::vec2{glm::cos(angle), glm::sin(angle)};
        }

        const auto& scales = SimulationConfig::InitialBodies::scales;

        std::array<float, bodyCount> masses{};

        glm::vec2 centerOfMass{0.0f};

        float totalMass = 0.0f;

        for (size_t i = 0; i < basePositions.size(); ++i)
        {
            Transform2DComponent probe{};
            probe.scale = glm::vec2{scales[i]};
            masses[i]   = massFromScale(probe);
            centerOfMass += masses[i] * basePositions[i];
            totalMass += masses[i];
        }

        if (totalMass > std::numeric_limits<float>::epsilon())
        {
            centerOfMass /= totalMass;
        }

        const float angularVelocity =
                glm::sqrt(gravityStrength * totalMass / (orbitRadius * orbitRadius * orbitRadius));

        const auto& colors = SimulationConfig::InitialBodies::colors;

        for (std::size_t i = 0; i < basePositions.size(); ++i)
        {
            auto            body         = GameObject::createGameObjectWithId();
            const glm::vec2 centered     = basePositions[i] - centerOfMass;
            body.transform2d.scale       = glm::vec2{scales[i]};
            body.transform2d.translation = centered;
            const glm::vec2 tangential   = {-centered.y, centered.x};
            body.rigidBody2d.velocity    = angularVelocity * tangential;
            body.rigidBody2d.mass        = masses[i];
            body.color                   = colors[i];
            body.model                   = circleModel;
            physicsObjects.push_back(std::move(body));
            baseColors.push_back(colors[i]);
        }

        std::vector<float> trailSpawnAccumulator(physicsObjects.size(), 0.0f);

        // create vector field
        std::vector<GameObject> vectorField{};
        const int               gridCount = SimulationConfig::VectorField::gridCount;
        for (int i = 0; i < gridCount; i++)
        {
            for (int j = 0; j < gridCount; j++)
            {
                auto vf              = GameObject::createGameObjectWithId();
                vf.transform2d.scale = glm::vec2{SimulationConfig::VectorField::baseScale};
                float xPos           = -1.0f +
                             (static_cast<float>(i) + 0.5f) * 2.0f / static_cast<float>(gridCount);
                float yPos = -1.0f +
                             (static_cast<float>(j) + 0.5f) * 2.0f / static_cast<float>(gridCount);
                vf.transform2d.translation = {xPos, yPos};
                vf.color                   = glm::vec3(1.0f);
                vf.model                   = squareModel;
                vectorField.push_back(std::move(vf));
            }
        }

        Vec2FieldSystem vecFieldSystem{};
        TrailSystem     trailSystem{trailQuadModel};

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass()};

        while (!window.shouldClose())
        {
            // Poll for window events
            glfwPollEvents();

            if (auto commandBuffer = renderer.beginFrame())
            {
                for (auto& obj : physicsObjects)
                {
                    obj.rigidBody2d.mass = massFromScale(obj.transform2d);
                }
                // update systems
                constexpr float frameDt = SimulationConfig::Physics::frameDt;
                gravitySystem.update(physicsObjects,
                                     frameDt,
                                     SimulationConfig::Physics::gravitySubsteps);
                vecFieldSystem.update(gravitySystem, physicsObjects, vectorField);
                handleCollisions(physicsObjects, baseColors, trailSpawnAccumulator, circleModel);

                updateColorsAndTrails(physicsObjects,
                                      baseColors,
                                      trailSpawnAccumulator,
                                      trailSystem,
                                      frameDt);

                trailSystem.update(frameDt);

                // render system
                renderer.beginSwapChainRenderPass(commandBuffer);

                simpleRenderSystem.renderGameObjects(commandBuffer, vectorField);
                if (const auto& trailRenderObjects = trailSystem.renderObjects();
                    !trailRenderObjects.empty())
                {
                    simpleRenderSystem.renderGameObjects(commandBuffer, trailRenderObjects);
                }
                simpleRenderSystem.renderGameObjects(commandBuffer, physicsObjects);
                renderer.endSwapChainRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }

        // Wait for the device to finish operations before exiting
        vkDeviceWaitIdle(device.device());
    }

    void App::loadGameObjects() const
    {
        /* Intentionally empty: this demo seeds runtime objects in run(). */
    }

} // namespace engine