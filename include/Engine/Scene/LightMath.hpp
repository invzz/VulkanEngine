#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_LIGHTMATH_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_LIGHTMATH_HPP

#include <algorithm>
#include <cmath>
#include <limits>

#include "Engine/Scene/components/SpotLightComponent.hpp"

namespace engine {

    inline float computeSpotLightRadius2(const SpotLightComponent& spotLight) {
        constexpr float kMinContribution = 0.01f;

        const float target = std::max(spotLight.intensity / kMinContribution, 0.0f);

        const float c = std::max(spotLight.constantAttenuation, 0.0f);
        const float l = std::max(spotLight.linearAttenuation, 0.0f);
        const float q = std::max(spotLight.quadraticAttenuation, 0.0f);

        if (l == 0.0f && q == 0.0f) {
            return std::numeric_limits<float>::infinity();
        }

        const float A = q;
        const float B = l;
        const float C = c - target;

        if (A == 0.0f) {
            if (B == 0.0f)
                return std::numeric_limits<float>::infinity();
            const float d = (target - c) / B;
            return d > 0.0f ? d * d : 0.0f;
        }

        const float discriminant = (B * B) - (4.0f * A * C);
        if (discriminant <= 0.0f) {
            return 0.0f;
        }

        const float sqrtD = std::sqrt(discriminant);
        const float d0    = (-B + sqrtD) / (2.0f * A);
        const float d1    = (-B - sqrtD) / (2.0f * A);
        const float d     = std::max(d0, d1);

        return d > 0.0f ? d * d : 0.0f;
    }

}  // namespace engine

#endif
