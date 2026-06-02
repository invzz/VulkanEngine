#include "Editor/Infrastructure/PhysicsRuntimeAdapter.hpp"

#include "Engine/Systems/JoltPhysicsSystem.hpp"

namespace engine {

PhysicsRuntimeAdapter::PhysicsRuntimeAdapter(JoltPhysicsSystem* physicsSystem)
    : physicsSystem_(physicsSystem) {}

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

}  // namespace engine
