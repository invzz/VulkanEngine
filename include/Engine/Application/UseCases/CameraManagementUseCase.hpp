#pragma once

#include "Engine/Application/Ports/ICameraPort.hpp"
#include "Engine/Application/SceneRuntimeState.hpp"

namespace engine {

    // Use case for camera management (set active, query active).
    // Aggregates SetActiveCameraUseCase operations.
    class CameraManagementUseCase {
       public:
        explicit CameraManagementUseCase(ICameraPort& cameraPort);

        // Set the active camera entity in the runtime state.
        void setActiveCamera(entt::entity cameraEntity, SceneRuntimeState& runtimeState);

        // Get the current active camera entity from runtime state.
        [[nodiscard]] entt::entity getActiveCamera(const SceneRuntimeState& runtimeState) const;

       private:
        ICameraPort& cameraPort_;
    };

}  // namespace engine
