#pragma once

#include "Engine/Application/Ports/ICameraPort.hpp"
#include "Engine/Application/SceneRuntimeState.hpp"

namespace engine {

    // Use case for setting the active camera.
    class SetActiveCameraUseCase {
       public:
        explicit SetActiveCameraUseCase(ICameraPort& cameraPort);

        // Set the active camera entity in the runtime state.
        void execute(entt::entity cameraEntity, SceneRuntimeState& runtimeState);

       private:
        ICameraPort& cameraPort_;
    };

}  // namespace engine
