#include "Engine/Application/UseCases/TogglePhysicsSimulationUseCase.hpp"

namespace engine {

TogglePhysicsSimulationUseCase::TogglePhysicsSimulationUseCase(IPhysicsRuntimePort& physicsRuntime)
    : physicsRuntime_(physicsRuntime) {}

void TogglePhysicsSimulationUseCase::execute(bool& simulationRunningRef) {
  simulationRunningRef = !simulationRunningRef;
}

void TogglePhysicsSimulationUseCase::setGroundEnabled(bool enabled) {
  physicsRuntime_.setGroundEnabled(enabled);
}

bool TogglePhysicsSimulationUseCase::isRunning() const {
  return physicsRuntime_.physicsSimulationRunningRef();
}

}  // namespace engine
