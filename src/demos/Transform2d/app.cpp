
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
#include <limits>

#include "Device.hpp"
#include "GameObject.hpp"
#include "Model.hpp"
#include "SierpinskiTriangle.hpp"
#include "SimpleRenderSystem.hpp"
#include "Window.hpp"
#include "app.hpp"

namespace engine {
    class GravityPhysicsSystem
    {
      public:
        explicit GravityPhysicsSystem(float strength) : strengthGravity{strength} {}

        const float strengthGravity;

        // dt stands for delta time, and specifies the amount of time to advance the simulation
        // substeps is how many intervals to divide the forward time step in. More substeps result
        // in a more stable simulation, but takes longer to compute
        void update(std::vector<GameObject>& objs, float dt, unsigned int substeps = 1) const
        {
            const float stepDelta = dt / static_cast<float>(substeps);
            for (int i = 0; i < substeps; i++)
            {
                stepSimulation(objs, stepDelta);
            }
        }

        glm::vec2 computeForce(GameObject& fromObj, GameObject& toObj) const
        {
            auto  offset          = fromObj.transform2d.translation - toObj.transform2d.translation;
            float distanceSquared = glm::dot(offset, offset);

            // clown town - just going to return 0 if objects are too close together...
            if (glm::abs(distanceSquared) < 1e-10f)
            {
                return {.0f, .0f};
            }

            float force = strengthGravity * toObj.rigidBody2d.mass * fromObj.rigidBody2d.mass /
                          distanceSquared;
            return force * offset / glm::sqrt(distanceSquared);
        }

      private:
        void stepSimulation(std::vector<GameObject>& physicsObjs, float dt) const
        {
            // Loops through all pairs of objects and applies attractive force between them
            for (auto iterA = physicsObjs.begin(); iterA != physicsObjs.end(); ++iterA)
            {
                auto& objA = *iterA;
                for (auto iterB = iterA; iterB != physicsObjs.end(); ++iterB)
                {
                    if (iterA == iterB) continue;
                    auto& objB = *iterB;

                    auto force = computeForce(objA, objB);
                    objA.rigidBody2d.velocity += dt * -force / objA.rigidBody2d.mass;
                    objB.rigidBody2d.velocity += dt * force / objB.rigidBody2d.mass;
                }
            }

            // update each objects position based on its final velocity
            for (auto& obj : physicsObjs)
            {
                obj.transform2d.translation += dt * obj.rigidBody2d.velocity;
            }
        }
    };

    class Vec2FieldSystem
    {
      public:
        void update(const GravityPhysicsSystem& physicsSystem,
                    std::vector<GameObject>&    physicsObjs,
                    std::vector<GameObject>&    vectorField) const
        {
            // For each field line we caluclate the net graviation force for that point in space
            for (auto& vf : vectorField)
            {
                glm::vec2 direction{};
                glm::vec3 blendedColor{};
                float     totalWeight = 0.0f;
                for (auto& obj : physicsObjs)
                {
                    auto force = physicsSystem.computeForce(obj, vf);
                    direction += force;

                    float weight = glm::length(force);
                    blendedColor += weight * obj.color;
                    totalWeight += weight;
                }

                // This scales the length of the field line based on the log of the length
                // values were chosen just through trial and error based on what i liked the look
                // of and then the field line is rotated to point in the direction of the field
                vf.transform2d.scale.x =
                        0.005f +
                        0.045f * glm::clamp(glm::log(glm::length(direction) + 1) / 3.f, 0.f, 1.f);
        vf.transform2d.rotation = glm::atan(direction.y, direction.x);

                if (totalWeight > std::numeric_limits<float>::epsilon())
                {
                    vf.color = blendedColor / totalWeight;
                }
            }
        }
    };

    std::unique_ptr<Model> createSquareModel(Device& device, glm::vec2 offset)
    {
        std::vector<Model::Vertex> vertices = {
                {{-0.5f, -0.5f}},
                {{0.5f, 0.5f}},
                {{-0.5f, 0.5f}},
                {{-0.5f, -0.5f}},
                {{0.5f, -0.5f}},
                {{0.5f, 0.5f}}, //
        };
        for (auto& v : vertices)
        {
            v.position += offset;
        }
        return std::make_unique<Model>(device, vertices);
    }

    std::unique_ptr<Model> createCircleModel(Device& device, unsigned int numSides)
    {
        std::vector<Model::Vertex> uniqueVertices{};
    const auto sideCount = static_cast<int>(numSides);
        for (int i = 0; i < sideCount; i++)
        {
            float angle = static_cast<float>(i) * glm::two_pi<float>() /
                          static_cast<float>(sideCount);
            uniqueVertices.push_back({{glm::cos(angle), glm::sin(angle)}});
        }
        uniqueVertices.push_back({}); // adds center vertex at 0, 0

        std::vector<Model::Vertex> vertices{};
        for (int i = 0; i < sideCount; i++)
        {
            vertices.push_back(uniqueVertices[i]);
            vertices.push_back(uniqueVertices[(i + 1) % sideCount]);
            vertices.push_back(uniqueVertices[sideCount]);
        }
        return std::make_unique<Model>(device, vertices);
    }

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
        std::shared_ptr<Model> squareModel =
                createSquareModel(device, {.5f, .0f}); // offset model by .5 so rotation occurs
                                                       // at edge rather than center of square
        std::shared_ptr<Model> circleModel = createCircleModel(device, 64);

        // create physics objects
        std::vector<GameObject> physicsObjects{};

        auto red                    = GameObject::createGameObjectWithId();
        red.transform2d.scale       = glm::vec2{.05f};
        red.transform2d.translation = {.5f, .5f};
        red.color                   = {1.f, 0.f, 0.f};
        red.rigidBody2d.velocity    = {-.5f, .0f};
        red.model                   = circleModel;
        physicsObjects.push_back(std::move(red));

        auto blue                    = GameObject::createGameObjectWithId();
        blue.transform2d.scale       = glm::vec2{.05f};
        blue.transform2d.translation = {-.45f, -.25f};
        blue.color                   = {0.f, 0.f, 1.f};
        blue.rigidBody2d.velocity    = {.5f, .0f};
        blue.model                   = circleModel;
        physicsObjects.push_back(std::move(blue));

        // create vector field
        std::vector<GameObject> vectorField{};
        int                     gridCount = 40;
        for (int i = 0; i < gridCount; i++)
        {
            for (int j = 0; j < gridCount; j++)
            {
                auto vf                    = GameObject::createGameObjectWithId();
                vf.transform2d.scale       = glm::vec2(0.005f);
                float xPos                 = -1.0f + (static_cast<float>(i) + 0.5f) * 2.0f /
                                                        static_cast<float>(gridCount);
                float yPos                 = -1.0f + (static_cast<float>(j) + 0.5f) * 2.0f /
                                                        static_cast<float>(gridCount);
                vf.transform2d.translation = {xPos, yPos};
                vf.color                   = glm::vec3(1.0f);
                vf.model                   = squareModel;
                vectorField.push_back(std::move(vf));
            }
        }

        GravityPhysicsSystem gravitySystem{0.81f};
        Vec2FieldSystem      vecFieldSystem{};

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass()};
        while (!window.shouldClose())
        {
            // Poll for window events
            glfwPollEvents();

            if (auto commandBuffer = renderer.beginFrame())
            {
                // update systems
                gravitySystem.update(physicsObjects, 1.f / 60, 5);
                vecFieldSystem.update(gravitySystem, physicsObjects, vectorField);

                // render system
                renderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(commandBuffer, physicsObjects);
                simpleRenderSystem.renderGameObjects(commandBuffer, vectorField);
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

} // namespace engine