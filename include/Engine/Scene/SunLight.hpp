#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SUNLIGHT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SUNLIGHT_HPP
#include <glm/glm.hpp>
#include <algorithm>
namespace engine {
    // Physically-derived sun helpers shared between the sky renderer and the
    // directional "sun" light so the two always agree.

    // Sun direction (unit vector, +Y = up) for a given time of day in [0,24).
    // Uses a proper solar position model driven by observer latitude and the
    // day of year (seasonal declination), so the sun rises/sets at the correct
    // compass bearing and the day arc shortens in winter.
    inline glm::vec3 sunDirectionFromTimeOfDay(float t, float latitudeDeg, float dayOfYear) {
        constexpr float DEG2RAD = 0.017453293f;
        constexpr float TWO_PI  = 6.2831853f;

        // Declination: sun's tilt relative to equator, driven by day of year.
        // Peaks at +23.44 deg near summer solstice (~day 172), -23.44 deg near
        // winter solstice (~day 355).
        const float declination = 23.44f * DEG2RAD *
            std::sinf(DEG2RAD * 360.0f * (284.0f + dayOfYear) / 365.0f);

        // Hour angle: 0 at solar noon, negative morning, positive afternoon, 15 deg/hr.
        const float hourAngle = (t - 12.0f) * 15.0f * DEG2RAD;

        const float lat = latitudeDeg * DEG2RAD;

        // Elevation via spherical law of cosines.
        const float sinElev = std::sinf(lat) * std::sinf(declination) +
                              std::cosf(lat) * std::cosf(declination) * std::cosf(hourAngle);
        const float elevation = std::asinf(std::clamp(sinElev, -1.0f, 1.0f));

        // Azimuth (measured from north, clockwise through east).
        float cosAz = (std::sinf(declination) - std::sinf(elevation) * std::sinf(lat)) /
                      (std::cosf(elevation) * std::cosf(lat) + 1e-6f);
        cosAz = std::clamp(cosAz, -1.0f, 1.0f);
        float azimuth = std::acosf(cosAz);
        if (hourAngle > 0.0f) azimuth = TWO_PI - azimuth;  // afternoon mirrors the morning half

        // North = -Z, East = +X, up = +Y (engine world axes).
        return glm::vec3(
            std::sinf(azimuth) * std::cosf(elevation),
            std::sinf(elevation),
            -std::cosf(azimuth) * std::cosf(elevation));
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
