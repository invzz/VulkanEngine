
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

#include "Device.hpp"
#include "GameObject.hpp"
#include "Model.hpp"
#include "SierpinskiTriangle.hpp"
#include "SimpleRenderSystem.hpp"
#include "Window.hpp"
#include "app.hpp"

namespace engine {
    namespace {
        struct BorderSettings
        {
            static constexpr float repulsionStart    = 0.78f;
            static constexpr float dampingStart      = 0.82f;
            static constexpr float clamp             = 0.97f;
            static constexpr float repulsionStrength = 12.5f;
            static constexpr float dampingStrength   = 6.5f;
            static constexpr float bounceFactor      = -0.25f;
        };

        float massFromScale(const Transform2DComponent& transform)
        {
            const glm::vec2 absScale = glm::abs(transform.scale);
            const float     area     = absScale.x * absScale.y;
            constexpr float density  = 400.0f; // tuned so radius 0.05 discs keep mass ≈ 1
            return glm::max(area * density, 1e-4f);
        }

        float edgeExposure(const glm::vec2& position)
        {
            const float maxAbs = glm::max(glm::abs(position.x), glm::abs(position.y));
            if (maxAbs <= BorderSettings::repulsionStart) return 0.0f;
            const float range = BorderSettings::clamp - BorderSettings::repulsionStart;
            if (range <= std::numeric_limits<float>::epsilon()) return 1.0f;
            return glm::clamp((maxAbs - BorderSettings::repulsionStart) / range, 0.0f, 1.0f);
        }
    } // namespace

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
                applyBoundaryDamping(obj, dt);
            }
        }

        void applyBoundaryDamping(GameObject& obj, float dt) const
        {
            auto& pos = obj.transform2d.translation;
            auto& vel = obj.rigidBody2d.velocity;

            for (int axis = 0; axis < 2; ++axis)
            {
                float& coord    = axis == 0 ? pos.x : pos.y;
                float& velocity = axis == 0 ? vel.x : vel.y;
                float  absCoord = glm::abs(coord);
                float  side     = coord >= 0.f ? 1.f : -1.f;

                if (absCoord > BorderSettings::repulsionStart)
                {
                    float repelT = glm::clamp(
                            (absCoord - BorderSettings::repulsionStart) /
                                    (BorderSettings::clamp - BorderSettings::repulsionStart),
                            0.f,
                            1.f);
                    float repelAccel = BorderSettings::repulsionStrength * (0.25f + 0.75f * repelT);
                    velocity -= side * repelAccel * dt;
                }

                if (absCoord > BorderSettings::dampingStart)
                {
                    float t = glm::clamp(
                            (absCoord - BorderSettings::dampingStart) /
                                    (BorderSettings::clamp - BorderSettings::dampingStart),
                            0.f,
                            1.f);
                    float dampingFactor =
                            glm::max(0.f, 1.f - BorderSettings::dampingStrength * t * dt);
                    velocity *= dampingFactor;
                }

                if (absCoord > BorderSettings::clamp)
                {
                    coord = side * BorderSettings::clamp;
                    if (velocity * side > 0.f)
                    {
                        velocity *= BorderSettings::bounceFactor;
                    }
                }
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

    class TrailSystem
    {
      public:
        explicit TrailSystem(std::shared_ptr<Model> model) : trailModel(std::move(model)) {}

        void spawn(const glm::vec2& position, const glm::vec2& velocity, const glm::vec3& color)
        {
            const float speed = glm::length(velocity);
            if (speed < std::numeric_limits<float>::epsilon()) return;

            GameObject particle              = GameObject::createGameObjectWithId();
            particle.model                   = trailModel;
            particle.transform2d.translation = position;
            particle.transform2d.rotation    = glm::atan(velocity.y, velocity.x);

            const float speedRatio     = glm::clamp(speed / speedForMaxScale, 0.f, 1.f);
            const float length         = glm::mix(minLength, maxLength, speedRatio);
            const float width          = glm::mix(minWidth, maxWidth, speedRatio);
            particle.transform2d.scale = {width, length};
            particle.color             = color;

            particles.push_back(std::move(particle));
            ages.push_back(0.f);
            const float lifetimeRatio = glm::clamp(speed / speedForMaxLifetime, 0.f, 1.f);
            lifetimes.push_back(glm::mix(minLifetime, maxLifetime, lifetimeRatio));
            baseColors.push_back(color);
        }

        void update(float dt)
        {
            for (size_t i = 0; i < particles.size(); ++i)
            {
                ages[i] += dt;
                float     normalizedAge = glm::clamp(1.f - ages[i] / lifetimes[i], 0.f, 1.f);
                glm::vec3 fadedColor    = normalizedAge * baseColors[i];
                particles[i].color      = fadedColor;
                particles[i].transform2d.scale *= trailShrinkFactor;
            }

            for (size_t i = particles.size(); i-- > 0;)
            {
                if (ages[i] >= lifetimes[i])
                {
                    removeAt(i);
                }
            }
        }

        const std::vector<GameObject>& renderObjects() const { return particles; }

      private:
        void removeAt(size_t index)
        {
            particles[index] = std::move(particles.back());
            particles.pop_back();

            ages[index] = ages.back();
            ages.pop_back();

            lifetimes[index] = lifetimes.back();
            lifetimes.pop_back();

            baseColors[index] = baseColors.back();
            baseColors.pop_back();
        }

        std::shared_ptr<Model>  trailModel;
        std::vector<GameObject> particles{};
        std::vector<float>      ages{};
        std::vector<float>      lifetimes{};
        std::vector<glm::vec3>  baseColors{};

        static constexpr float minWidth            = 0.0035f;
        static constexpr float maxWidth            = 0.0065f;
        static constexpr float minLength           = 0.010f;
        static constexpr float maxLength           = 0.028f;
        static constexpr float minLifetime         = 0.55f;
        static constexpr float maxLifetime         = 1.65f;
        static constexpr float speedForMaxScale    = 1.4f;
        static constexpr float speedForMaxLifetime = 1.1f;
        static constexpr float trailShrinkFactor   = 0.985f;
    };

    void updateColorsAndTrails(std::vector<GameObject>&      physicsObjects,
                               const std::vector<glm::vec3>& baseColors,
                               std::vector<float>&           trailSpawnAccumulator,
                               TrailSystem&                  trailSystem,
                               float                         frameDt)
    {
        const float     minIntensity      = 0.35f;
        const float     intensityMaxSpeed = 1.2f;
        constexpr float spawnInterval     = 0.018f;
        constexpr float minTrailSpeed     = 0.05f;
        constexpr float epsilon           = std::numeric_limits<float>::epsilon();

        for (size_t i = 0; i < physicsObjects.size(); ++i)
        {
            auto& obj   = physicsObjects[i];
            float speed = glm::length(obj.rigidBody2d.velocity);
            float t     = glm::clamp(speed / intensityMaxSpeed, 0.f, 1.f);
            float scale = glm::mix(minIntensity, 1.0f, t);

            glm::vec3 pastel     = glm::mix(glm::vec3(0.9f), baseColors[i], 0.4f);
            glm::vec3 speedTint  = glm::mix(pastel, baseColors[i], t);
            float     edgeHeat   = glm::pow(edgeExposure(obj.transform2d.translation), 1.2f);
            glm::vec3 glowTarget = glm::mix(speedTint, glm::vec3(1.0f), edgeHeat);
            obj.color            = glm::clamp(scale * glowTarget, glm::vec3(0.0f), glm::vec3(1.0f));

            trailSpawnAccumulator[i] += frameDt;
            if (speed <= minTrailSpeed + epsilon)
            {
                trailSpawnAccumulator[i] = glm::min(trailSpawnAccumulator[i], spawnInterval);
                continue;
            }

            const auto spawnCount = static_cast<int>(trailSpawnAccumulator[i] / spawnInterval);
            if (spawnCount <= 0) continue;

            trailSpawnAccumulator[i] -= spawnInterval * static_cast<float>(spawnCount);
            for (int emit = 0; emit < spawnCount; ++emit)
            {
                trailSystem.spawn(obj.transform2d.translation, obj.rigidBody2d.velocity, obj.color);
            }
        }
    }

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
        const auto                 sideCount = static_cast<int>(numSides);
        for (int i = 0; i < sideCount; i++)
        {
            float angle =
                    static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(sideCount);
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
        std::shared_ptr<Model> circleModel    = createCircleModel(device, 64);
        std::shared_ptr<Model> trailQuadModel = createSquareModel(device, {0.0f, 0.0f});

        const float          gravityStrength = 0.61f;
        GravityPhysicsSystem gravitySystem{gravityStrength};

        // create physics objects
        std::vector<GameObject>        physicsObjects{};
        std::vector<glm::vec3>         baseColors{};
        const float                    orbitRadius   = 0.35f; // circumradius of the setup
        const auto                     sqrt3         = glm::sqrt(3.0f);
        const float                    sideLength    = sqrt3 * orbitRadius;
        const std::array<glm::vec2, 3> basePositions = {
                glm::vec2{orbitRadius, 0.0f},
                glm::vec2{-0.5f * orbitRadius, 0.5f * sqrt3 * orbitRadius},
                glm::vec2{-0.5f * orbitRadius, -0.5f * sqrt3 * orbitRadius},
        };
        const std::array<float, 3> scales = {
                0.036f,
                0.028f,
                0.022f,
        };
        std::array<float, 3> masses{};
        glm::vec2            centerOfMass{0.0f};
        float                totalMass = 0.0f;
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
                glm::sqrt(gravityStrength * totalMass / (sideLength * sideLength * sideLength));
        const std::array<glm::vec3, 3> colors = {
                glm::vec3{1.0f, 0.2f, 0.2f},
                glm::vec3{0.2f, 1.0f, 0.4f},
                glm::vec3{0.2f, 0.4f, 1.0f},
        };

        for (size_t i = 0; i < basePositions.size(); ++i)
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
        int                     gridCount = 40;
        for (int i = 0; i < gridCount; i++)
        {
            for (int j = 0; j < gridCount; j++)
            {
                auto vf              = GameObject::createGameObjectWithId();
                vf.transform2d.scale = glm::vec2(0.005f);
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
                constexpr float frameDt = 1.f / 120.f;
                gravitySystem.update(physicsObjects, frameDt, 5);
                vecFieldSystem.update(gravitySystem, physicsObjects, vectorField);

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