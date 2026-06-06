#pragma once

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"

namespace engine {

    // Use case for toggling physics simulation.
    class TogglePhysicsSimulationUseCase {
       public:
        explicit TogglePhysicsSimulationUseCase(IPhysicsRuntimePort& physicsRuntime);

        // Toggle physics simulation running state.
        void execute(bool& simulationRunningRef);

        // Set ground enabled state.
        void setGroundEnabled(bool enabled);

        // Get current simulation running state.
        [[nodiscard]] bool isRunning() const;

       private:
        IPhysicsRuntimePort& physicsRuntime_;
    };

}  // namespace engine
