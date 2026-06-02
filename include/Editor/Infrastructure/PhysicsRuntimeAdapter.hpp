#pragma once

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"

namespace engine {

class JoltPhysicsSystem;

class PhysicsRuntimeAdapter final : public IPhysicsRuntimePort {
 public:
  explicit PhysicsRuntimeAdapter(JoltPhysicsSystem* physicsSystem);

  [[nodiscard]] bool& physicsSimulationRunningRef() override;
  void clearSceneBodies() override;
  void setGroundEnabled(bool enabled) override;
  [[nodiscard]] JoltPhysicsSystem* joltPhysicsSystem() const override;

 private:
  JoltPhysicsSystem* physicsSystem_ = nullptr;
};

}  // namespace engine
