#pragma once

#include <array>
#include <cstddef>
#include <glm/vec3.hpp>

namespace engine {

    class SimulationConfig
    {
      public:
        struct Physics
        {
            static constexpr float        gravityStrength = 0.6f;
            static constexpr unsigned int gravitySubsteps = 5;
            static constexpr float        frameDt         = 1.f / 120.f;
            static constexpr float        massDensity     = 300.0f;
            static constexpr float        minScale        = 0.010f;
            static constexpr float        maxScale        = 0.28f;
            static constexpr float        minSplitScale   = 0.018f;
        };

        struct InitialBodies
        {
            static constexpr std::size_t                 count       = 10;
            static constexpr float                       orbitRadius = 0.35f;
            static inline const std::array<float, count> scales      = {
                    0.06f,
                    0.05f,
                    0.040f,
                    0.036f,
                    0.030f,
                    0.028f,
                    0.024f,
                    0.022f,
                    0.020f,
                    0.018f,
            };

            static inline const std::array<glm::vec3, count> colors = {
                    glm::vec3{1.0f, 0.25f, 0.25f},
                    glm::vec3{0.25f, 1.0f, 0.45f},
                    glm::vec3{0.25f, 0.45f, 1.0f},
                    glm::vec3{1.0f, 0.7f, 0.3f},
                    glm::vec3{0.7f, 0.3f, 1.0f},
                    glm::vec3{0.3f, 1.0f, 0.7f},
                    glm::vec3{1.0f, 1.0f, 0.4f},
                    glm::vec3{0.4f, 0.4f, 1.0f},
                    glm::vec3{0.7f, 1.0f, 0.3f},
                    glm::vec3{1.0f, 0.3f, 0.7f},

            };
        };

        struct VectorField
        {
            static constexpr int   gridCount          = 40;
            static constexpr float baseScale          = 0.007f;
            static constexpr float dynamicScale       = 0.045f;
            static constexpr float logScaleNormalizer = 3.f;
        };

        struct Collision
        {
            static constexpr float splitSpeedThreshold = 2.81f;
            static constexpr float splitOffsetFactor   = 1.35f;
            static constexpr float splitImpulse        = 0.55f;
        };

        struct Trails
        {
            static constexpr float minWidth            = 0.0035f;
            static constexpr float maxWidth            = 0.0065f;
            static constexpr float minLength           = 0.010f;
            static constexpr float maxLength           = 0.028f;
            static constexpr float minLifetime         = 0.55f;
            static constexpr float maxLifetime         = 1.65f;
            static constexpr float speedForMaxScale    = 1.4f;
            static constexpr float speedForMaxLifetime = 1.1f;
            static constexpr float shrinkFactor        = 0.985f;
            static constexpr float spawnInterval       = 0.008f;
            static constexpr float minTrailSpeed       = 0.05f;
            static constexpr float minEmitterScale     = 0.054f;
            static constexpr float minIntensity        = 0.35f;
            static constexpr float intensityMaxSpeed   = 1.2f;
            static constexpr float pastelMix           = 0.4f;
            static constexpr float edgeGlowExponent    = 1.2f;
        };

        struct Borders
        {
            static constexpr float repulsionStart    = 0.78f;
            static constexpr float dampingStart      = 0.82f;
            static constexpr float clamp             = 0.97f;
            static constexpr float repulsionStrength = 12.5f;
            static constexpr float dampingStrength   = 6.5f;
            static constexpr float bounceFactor      = -0.25f;
        };
    };

    using BorderSettings = SimulationConfig::Borders;

} // namespace engine
