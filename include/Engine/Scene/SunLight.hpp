#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SUNLIGHT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SUNLIGHT_HPP
#include <glm/glm.hpp>
namespace engine {
    // Physically-derived sun helpers shared between the sky renderer and the
    // directional "sun" light so the two always agree.

    // Sun direction (unit vector, +Y = up) for a given time of day in [0,24).
    // Sunrise at t=6, zenith at t=12, sunset at t=18, midnight at t=0/24.
    inline glm::vec3 sunDirectionFromTimeOfDay(float t) {
        const float elev     = std::sinf((t - 6.0f) / 24.0f * 6.2831853f);
        const float cosElev  = std::sqrtf(std::max(1.0f - elev * elev, 0.0f));
        const float azimuth  = (t - 6.0f) / 24.0f * 6.2831853f;
        return glm::vec3(cosElev * std::cosf(azimuth), elev, cosElev * std::sinf(azimuth));
    }

    // Chromatic transmittance of direct sunlight travelling from the ground
    // viewer to the top of the atmosphere along sunDir. Reddens at the horizon,
    // white at the zenith; returns (0,0,0) when the sun is below the local
    // horizon. Mirrors GetSunAttenuation() in sky_lut.comp exactly.
    inline glm::vec3 computeSunDirectColor(const glm::vec3& sunDir,
        float atmosphereRadius,
        const glm::vec3& betaRayleigh,
        const glm::vec3& betaMie,
        float rayleighScaleHeight,
        float mieScaleHeight,
        float groundRadius = 6360e3f) {
        const glm::vec3 viewerPos(0.0f, groundRadius, 0.0f);

        auto raySphereExit = [](const glm::vec3& ro, const glm::vec3& rd, float radius) -> float {
            const float b   = glm::dot(ro, rd);
            const float c   = glm::dot(ro, ro) - radius * radius;
            const float disc = b * b - c;
            if (disc < 0.0f) return -1.0f;
            const float s = std::sqrt(disc);
            const float t1 = -b - s;
            const float t2 = -b + s;
            if (t2 > 0.0f) return t2;
            if (t1 > 0.0f) return t1;
            return -1.0f;
        };

        const float tTop    = raySphereExit(viewerPos, sunDir, atmosphereRadius);
        const float tGround = raySphereExit(viewerPos, sunDir, groundRadius);
        if (tGround > 0.0f && (tTop < 0.0f || tGround < tTop)) {
            return glm::vec3(0.0f);  // sun below the local horizon
        }
        const float tMax = (tTop > 0.0f) ? tTop : 1e9f;
        if (tMax < 1e-6f) return glm::vec3(1.0f);

        const float HR = std::max(rayleighScaleHeight, 1.0f);
        const float HM = std::max(mieScaleHeight, 1.0f);
        const int   steps    = sunDir.y > 0.1f ? 8 : (sunDir.y > -0.1f ? 16 : 32);
        const float stepSize = tMax / static_cast<float>(steps);
        float odR = 0.0f, odM = 0.0f;
        for (int i = 0; i < steps; ++i) {
            const glm::vec3 p = viewerPos + sunDir * ((static_cast<float>(i) + 0.5f) * stepSize);
            const float h = glm::length(p) - groundRadius;
            if (h > 0.0f) {
                odR += std::exp(-h / HR) * stepSize;
                odM += std::exp(-h / HM) * stepSize;
            }
        }
        return glm::exp(-(betaRayleigh * odR + betaMie * odM));
    }
}  // namespace engine
#endif
