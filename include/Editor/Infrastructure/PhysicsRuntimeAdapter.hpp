#pragma once

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"

namespace engine {

class JoltPhysicsSystem;

class PhysicsRuntimeAdapter final : public IPhysicsRuntimePort {
 public:
  explicit PhysicsRuntimeAdapter(JoltPhysicsSystem* physicsSystem);

  void clearSceneBodies() override;
  void setGroundEnabled(bool enabled) override;

 private:
  JoltPhysicsSystem* physicsSystem_ = nullptr;
};

}  // namespace engine
