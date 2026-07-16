#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_ENVIRONMENTLIGHTINGSERVICE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_ENVIRONMENTLIGHTINGSERVICE_HPP

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "Engine/Systems/ProceduralSkyCapture.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

namespace engine {
    class Device;
    class Scene;
    class IBLSystem;
    class DescriptorManager;
    class Skybox;

    /**
     * @brief Owns environment/IBL lighting policy: procedural-sky capture and
     *        bake gating, the Yokohama-disk fallback, and driving the "sun"
     *        directional light from SkyboxSettings. Extracted from EngineState
     *        (which previously held 14 hysteresis fields + the bake logic) so
     *        the composition root stays lean.
     *
     * Lifetime note: construct with the long-lived Scene/Device, then call
     * init() once IBLSystem and DescriptorManager are created (mirrors
     * EngineState's init phases).
     */
    class EnvironmentLightingService {
       public:
        EnvironmentLightingService(Device& device, Scene& scene) : device_(device), scene_(scene) {}
        ~EnvironmentLightingService() = default;
        EnvironmentLightingService(const EnvironmentLightingService&)            = delete;
        EnvironmentLightingService& operator=(const EnvironmentLightingService&) = delete;

        void init(IBLSystem& ibl, DescriptorManager& descriptors);

        /// Sync the visible skybox + IBL environment with the current settings.
        /// `show` is the "Show Skybox" toggle.
        void syncEnvironmentLighting(bool show);
        /// Drive the directional light flagged as the sun from timeOfDay/latitude.
        void updateSunLight();

        bool      loadIBL(const char* path);
        void      resetIBLToFallback();

        SkyboxSettings& skySettings() { return skySettings_; }
        const SkyboxSettings& skySettings() const { return skySettings_; }
        std::unique_ptr<Skybox>& skybox() { return skybox_; }
        [[nodiscard]] Skybox*   skyboxPtr() const { return skybox_.get(); }
        [[nodiscard]] IBLSystem* iblSystem() const { return ibl_; }

       private:
        /// Re-bake the procedural sky to a cubemap and feed the IBL system,
        /// gated by hysteresis (timeOfDay accumulation) + dead-bands for the
        /// other driving parameters so a smoothly advancing day/night cycle
        /// does not re-bake every frame.
        void captureProceduralSkyToIBL();
        /// Commit the IBL descriptor images into the deferred lighting sets.
        void writeIBLDescriptorsToSets();

        Device&           device_;
        Scene&            scene_;
        IBLSystem*        ibl_         = nullptr;
        DescriptorManager* descriptors_ = nullptr;

        std::unique_ptr<Skybox>              skybox_;
        std::unique_ptr<class ProceduralSkyCapture> procSkyCapture_;
        SkyboxSettings                       skySettings_{};
        uint64_t                             iblGeneration_ = 0;

        // Procedural-sky IBL bake gating: hysteresis for continuous drivers
        // (timeOfDay) plus explicit dead-bands / dirty flags for the rest.
        float      procIblLat_         = 1e9f;   // last baked latitude (deg)
        int        procIblDay_         = -1;     // last baked day-of-year
        float      procIblPendingTime_ = 0.0f;   // accumulated |dtimeOfDay| since last bake
        float      procIblSampledTime_ = -1e9f;  // anchor for per-frame time accumulation
        double     procIblAtmoR_       = 0.0;    // atmosphereRadius
        double     procIblRayH_        = 0.0;    // rayleighScaleHeight
        double     procIblMieH_        = 0.0;    // mieScaleHeight
        glm::dvec3 procIblBetaR_       = {};     // betaRayleigh
        glm::dvec3 procIblBetaM_       = {};     // betaMie
        float      procIblMieG_        = -1.0f;  // mieG
        float      procIblSkyInt_      = -1.0f;  // skyIntensity
        int        bakeDeferredFrames_ = 30;     // skip first N frames (loader burst)
    };
}  // namespace engine

#endif
