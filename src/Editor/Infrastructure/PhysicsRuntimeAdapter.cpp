#include "Editor/Infrastructure/PhysicsRuntimeAdapter.hpp"

#include "Engine/Systems/JoltPhysicsSystem.hpp"

namespace engine {

PhysicsRuntimeAdapter::PhysicsRuntimeAdapter(JoltPhysicsSystem* physicsSystem)
    : physicsSystem_(physicsSystem) {}

bool& PhysicsRuntimeAdapter::physicsSimulationRunningRef() {
  // This is a placeholder - in reality this would reference EngineState's state
  static bool physicsSimulationRunning = false;
  return physicsSimulationRunning;
}

void PhysicsRuntimeAdapter::clearSceneBodies() {
  if (physicsSystem_ != nullptr) {
    physicsSystem_->clear();
  }
}

void PhysicsRuntimeAdapter::setGroundEnabled(bool enabled) {
  if (physicsSystem_ != nullptr) {
    physicsSystem_->setGroundEnabled(enabled);
  }
}

JoltPhysicsSystem* PhysicsRuntimeAdapter::joltPhysicsSystem() const {
  return physicsSystem_;
}

}  // namespace engine
