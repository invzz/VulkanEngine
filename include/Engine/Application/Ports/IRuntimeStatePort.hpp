#pragma once

namespace engine {

class PostProcessPushConstants;

// Port for runtime state access without knowing EngineState internals.
class IRuntimeStatePort {
 public:
  virtual ~IRuntimeStatePort() = default;

  [[nodiscard]] virtual bool& showGridRef() = 0;
  [[nodiscard]] virtual bool& showDebugObjectsRef() = 0;
  [[nodiscard]] virtual bool& showColliderWireframesRef() = 0;
  [[nodiscard]] virtual bool& physicsSimulationRunningRef() = 0;
  [[nodiscard]] virtual PostProcessPushConstants& postProcessPushRef() = 0;
};

}  // namespace engine
