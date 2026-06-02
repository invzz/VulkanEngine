#pragma once

#include "Engine/Application/Ports/IRuntimeStatePort.hpp"

namespace engine {

class EngineState;
class PostProcessPushConstants;

// Adapter that bridges EngineState to the runtime state port.
class RuntimeStateAdapter final : public IRuntimeStatePort {
 public:
  explicit RuntimeStateAdapter(EngineState& engineState);

  [[nodiscard]] bool& showGridRef() override;
  [[nodiscard]] PostProcessPushConstants& postProcessPushRef() override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
