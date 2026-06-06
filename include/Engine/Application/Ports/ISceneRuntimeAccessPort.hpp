#pragma once

#include <cstdint>

namespace engine {

    class Scene;
    class SkyboxSettings;
    class ShadowSettings;
    class PostProcessPushConstants;

    // Port for delivery to access scene runtime state without knowing EngineState internals.
    class ISceneRuntimeAccessPort {
       public:
        virtual ~ISceneRuntimeAccessPort() = default;

        [[nodiscard]] virtual Scene*                    scene()                    = 0;
        [[nodiscard]] virtual bool*                     showSkybox()               = 0;
        [[nodiscard]] virtual bool*                     showGrid()                 = 0;
        [[nodiscard]] virtual bool*                     showDebugObjects()         = 0;
        [[nodiscard]] virtual bool*                     physicsSimulationRunning() = 0;
        [[nodiscard]] virtual bool*                     showColliderWireframes()   = 0;
        [[nodiscard]] virtual bool*                     solidGroundEnabled()       = 0;
        [[nodiscard]] virtual SkyboxSettings*           skySettings()              = 0;
        [[nodiscard]] virtual ShadowSettings*           shadowSettings()           = 0;
        [[nodiscard]] virtual PostProcessPushConstants* postProcessPush()          = 0;
    };

}  // namespace engine
