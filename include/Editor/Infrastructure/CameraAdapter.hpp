#pragma once

#include "Engine/Application/Ports/ICameraPort.hpp"

namespace engine {

    class EngineState;

    // Adapter that bridges EngineState to the camera port.
    class CameraAdapter final : public ICameraPort {
       public:
        explicit CameraAdapter(EngineState& engineState);

        void                       setActiveCamera(entt::entity cameraEntity, SceneRuntimeState& runtimeState) override;
        [[nodiscard]] entt::entity getActiveCamera(const SceneRuntimeState& runtimeState) const override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
