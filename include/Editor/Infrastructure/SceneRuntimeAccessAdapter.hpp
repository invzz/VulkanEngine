#pragma once

#include "Engine/Application/Ports/ISceneRuntimeAccessPort.hpp"

namespace engine {

    class EngineState;

    // Adapter that bridges EngineState to the scene runtime access port.
    class SceneRuntimeAccessAdapter final : public ISceneRuntimeAccessPort {
       public:
        explicit SceneRuntimeAccessAdapter(EngineState& engineState);

        [[nodiscard]] Scene*                    scene() override;
        [[nodiscard]] bool*                     showSkybox() override;
        [[nodiscard]] bool*                     showGrid() override;
        [[nodiscard]] bool*                     showDebugObjects() override;
        [[nodiscard]] bool*                     physicsSimulationRunning() override;
        [[nodiscard]] bool*                     showColliderWireframes() override;
        [[nodiscard]] bool*                     solidGroundEnabled() override;
        [[nodiscard]] SkyboxSettings*           skySettings() override;
        [[nodiscard]] ShadowSettings*           shadowSettings() override;
        [[nodiscard]] PostProcessPushConstants* postProcessPush() override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
