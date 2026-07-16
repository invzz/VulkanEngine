#include "Engine/Systems/EnvironmentLightingService.hpp"

#include <cmath>

#include "Engine/Graphics/DescriptorManager.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/SunLight.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ProceduralSkyCapture.hpp"

namespace engine {
    void EnvironmentLightingService::init(IBLSystem& ibl, DescriptorManager& descriptors) {
        ibl_         = &ibl;
        descriptors_ = &descriptors;
    }

    void EnvironmentLightingService::syncEnvironmentLighting(bool show) {
        auto const& s = skySettings();
        if (s.proceduralSky) {
            // Procedural sky drives IBL independently of whether the sky is drawn
            // as the visible background. Bake it to a cubemap and feed the IBL
            // system so surfaces get diffuse (irradiance) + specular (prefiltered)
            // ambient terms, even when "Show Skybox" is off.
            captureProceduralSkyToIBL();
        } else if (show && !skybox_) {
            skybox_ = Skybox::loadFromFolder(device_, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");
            auto discard = ibl_->loadFromDisk(std::string(TEXTURE_PATH) + "/ibl/Yokohama");
        }
        // The visible background cubemap is only needed when explicitly shown and we
        // are not using the procedural sky.
        if (!s.proceduralSky && !show && skybox_) {
            skybox_.reset();
        }
        if (!s.proceduralSky && !show) {
            procSkyCapture_.reset();
            ibl_->resetToFallback();
        }
        ibl_->update();
        if (ibl_->getGenerationCounter() != iblGeneration_) {
            writeIBLDescriptorsToSets();
            iblGeneration_ = ibl_->getGenerationCounter();
        }
    }

    void EnvironmentLightingService::captureProceduralSkyToIBL() {
        // Defer the first bake(s) until the async model loader's initial GPU
        // burst has settled, to avoid racing its worker-thread submissions.
        if (bakeDeferredFrames_ > 0) {
            --bakeDeferredFrames_;
            return;
        }
        if (!procSkyCapture_) {
            procSkyCapture_ = std::make_unique<ProceduralSkyCapture>(device_);
        }
        auto const& s = skySettings();

        // --- Re-bake gating (hysteresis + explicit dirty checks) ---
        // Time-of-day accumulates per-frame so a smoothly advancing day/night
        // cycle does not re-trigger a full irradiance+prefilter regen every
        // frame from tiny steps. Latitude/day use a fixed dead-band. Scattering
        // params are rare manual tweaks -> always rebake immediately on change.
        constexpr float kTimeOfDayDelta = 0.05f;  // hours (accumulated)
        constexpr float kLatDelta       = 0.5f;   // degrees
        constexpr int   kDayDelta       = 1;      // day-of-year

        float const timeStep =
            (procIblSampledTime_ < -1e8f) ? (kTimeOfDayDelta + 1.0f)
                                          : std::fabs(s.timeOfDay - procIblSampledTime_);
        procIblSampledTime_ = s.timeOfDay;
        procIblPendingTime_ += timeStep;

        bool const timeMoved = procIblPendingTime_ > kTimeOfDayDelta;
        bool const latMoved  = std::fabs(s.latitude - procIblLat_) > kLatDelta;
        bool const dayMoved  = std::abs(s.dayOfYear - procIblDay_) > kDayDelta;
        bool const scatterMoved =
            (s.atmosphereRadius != procIblAtmoR_) ||
            (s.rayleighScaleHeight != procIblRayH_) ||
            (s.mieScaleHeight != procIblMieH_) ||
            glm::any(glm::notEqual(s.betaRayleigh, procIblBetaR_, 0.0)) ||
            glm::any(glm::notEqual(s.betaMie, procIblBetaM_, 0.0)) ||
            (s.mieG != procIblMieG_) ||
            (s.skyIntensity != procIblSkyInt_);

        if (!(timeMoved || latMoved || dayMoved || scatterMoved)) {
            return;
        }

        Skybox* captured = procSkyCapture_->capture(s, 256);
        ibl_->generateFromSkybox(*captured);

        // Commit anchors / dirty state.
        procIblLat_         = s.latitude;
        procIblDay_         = s.dayOfYear;
        procIblPendingTime_ = 0.0f;
        procIblSampledTime_ = s.timeOfDay;
        procIblAtmoR_       = s.atmosphereRadius;
        procIblRayH_        = s.rayleighScaleHeight;
        procIblMieH_        = s.mieScaleHeight;
        procIblBetaR_       = s.betaRayleigh;
        procIblBetaM_       = s.betaMie;
        procIblMieG_        = s.mieG;
        procIblSkyInt_      = s.skyIntensity;
    }

    void EnvironmentLightingService::updateSunLight() {
        auto&     s   = skySettings();
        auto&     reg = scene_.getRegistry();

        // Find the entity flagged as the sun light.
        entt::entity sunEntity = entt::null;
        auto         view      = reg.view<DirectionalLightComponent, TransformComponent>();
        for (auto e : view) {
            if (reg.get<DirectionalLightComponent>(e).isSun) {
                sunEntity = e;
                break;
            }
        }
        if (sunEntity == entt::null) {
            return;  // no sun light flagged; nothing to drive
        }

        const glm::vec3 sunDir = sunDirectionFromTimeOfDay(s.timeOfDay, s.latitude, static_cast<float>(s.dayOfYear));
        const float     elev   = sunDir.y;

        // Direction: the light's stored `direction` is the light's *travel*
        // direction (the deferred shader computes L = -direction, i.e. the
        // vector toward the sun). The procedural sky is authored in Y-up world
        // space, so the visible sun sits at world direction `sunDir`. To light
        // the hemisphere facing the visible sun, L must equal `sunDir`, hence
        // the light's travel direction is simply the opposite:
        //   direction = -sunDir
        // (The cubemap Y-flip in skybox_fullscreen.frag applies only to real
        // cubemap sampling, not to the procedural dome, so no flip here.)
        auto&           transform      = reg.get<TransformComponent>(sunEntity);
        const glm::vec3 lightTravelDir = sunDir;
        transform.lookAt(transform.translation + lightTravelDir);

        // Colour + night dimming, matching the sky shader exactly.
        glm::vec3 sunCol = computeSunDirectColor(sunDir,
            static_cast<float>(s.atmosphereRadius),
            glm::max(glm::vec3(s.betaRayleigh), glm::vec3(0.0f)),
            glm::max(glm::vec3(s.betaMie), glm::vec3(0.0f)),
            static_cast<float>(s.rayleighScaleHeight),
            static_cast<float>(s.mieScaleHeight));
        if (glm::all(glm::equal(sunCol, glm::vec3(0.0f)))) {
            sunCol = glm::vec3(1.0f, 0.35f, 0.1f);  // below horizon: warm ember
        }

        const float nightFactor        = glm::smoothstep(-0.05f, 0.15f, elev);
        const float effectiveIntensity = s.sunIntensity * glm::mix(0.02f, 1.0f, nightFactor);

        auto& dl     = reg.get<DirectionalLightComponent>(sunEntity);
        dl.color     = sunCol;
        dl.intensity = effectiveIntensity;
    }

    bool EnvironmentLightingService::loadIBL(const char* path) {
        return ibl_->loadFromDisk(std::string(path));
    }
    void EnvironmentLightingService::resetIBLToFallback() {
        ibl_->resetToFallback();
    }

    void EnvironmentLightingService::writeIBLDescriptorsToSets() {
        auto ir = ibl_->getIrradianceDescriptorInfo();
        auto pr = ibl_->getPrefilteredDescriptorInfo();
        auto br = ibl_->getBRDFLUTDescriptorInfo();
        for (auto& s : descriptors_->deferredIblDescriptorSets()) {
            DescriptorWriter(descriptors_->deferredIblSetLayout(), descriptors_->deferredIblPool())
                .writeImage(0, &ir)
                .writeImage(1, &pr)
                .writeImage(2, &br)
                .overwrite(s);
        }
    }
}  // namespace engine
