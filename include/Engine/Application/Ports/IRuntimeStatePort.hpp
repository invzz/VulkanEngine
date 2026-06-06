#pragma once

namespace engine {

    class PostProcessPushConstants;
    class SkyboxSettings;
    class ShadowSettings;

    // Port for runtime state access without knowing EngineState internals.
    class IRuntimeStatePort {
       public:
        virtual ~IRuntimeStatePort() = default;

        [[nodiscard]] virtual bool&                     showSkyboxRef()               = 0;
        [[nodiscard]] virtual bool&                     showGridRef()                 = 0;
        [[nodiscard]] virtual bool&                     showDebugObjectsRef()         = 0;
        [[nodiscard]] virtual bool&                     showColliderWireframesRef()   = 0;
        [[nodiscard]] virtual bool&                     physicsSimulationRunningRef() = 0;
        [[nodiscard]] virtual bool&                     solidGroundEnabledRef()       = 0;
        [[nodiscard]] virtual SkyboxSettings&           skySettingsRef()              = 0;
        [[nodiscard]] virtual ShadowSettings&           shadowSettingsRef()           = 0;
        [[nodiscard]] virtual PostProcessPushConstants& postProcessPushRef()          = 0;
    };

}  // namespace engine
