#pragma once

namespace engine {

class JoltPhysicsSystem;

class IPhysicsRuntimePort {
 public:
  virtual ~IPhysicsRuntimePort() = default;

  [[nodiscard]] virtual bool& physicsSimulationRunningRef() = 0;
  virtual void clearSceneBodies() = 0;
  virtual void setGroundEnabled(bool enabled) = 0;
  [[nodiscard]] virtual JoltPhysicsSystem* joltPhysicsSystem() const = 0;
};

}  // namespace engine
