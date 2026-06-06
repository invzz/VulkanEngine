#pragma once

#include "Engine/Application/SceneRuntimeState.hpp"

namespace engine {

    // Port for active camera management without knowing EngineState internals.
    class ICameraPort {
       public:
        virtual ~ICameraPort() = default;

        // Set the active camera entity via runtime state.
        virtual void setActiveCamera(entt::entity cameraEntity, SceneRuntimeState& runtimeState) = 0;

        // Get the current active camera entity from runtime state.
        [[nodiscard]] virtual entt::entity getActiveCamera(const SceneRuntimeState& runtimeState) const = 0;
    };

}  // namespace engine
