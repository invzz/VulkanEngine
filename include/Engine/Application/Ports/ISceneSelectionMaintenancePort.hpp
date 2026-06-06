#pragma once

#include "Engine/Application/SceneRuntimeState.hpp"

namespace engine {

    class ISceneSelectionMaintenancePort {
       public:
        virtual ~ISceneSelectionMaintenancePort() = default;

        virtual void processSelectionMaintenance(SceneRuntimeState& runtimeState) = 0;
    };

}  // namespace engine