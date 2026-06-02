#pragma once

namespace engine {

class IPhysicsRuntimePort {
 public:
  virtual ~IPhysicsRuntimePort() = default;

  virtual void clearSceneBodies() = 0;
  virtual void setGroundEnabled(bool enabled) = 0;
};

}  // namespace engine
