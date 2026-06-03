#pragma once

#include "Engine/Application/Ports/IRuntimeStatePort.hpp"

namespace engine {

class EngineState;
class PostProcessPushConstants;
class SkyboxSettings;
class ShadowSettings;

// Adapter that bridges EngineState to the runtime state port.
class RuntimeStateAdapter final : public IRuntimeStatePort {
 public:
  explicit RuntimeStateAdapter(EngineState& engineState);

  [[nodiscard]] bool& showSkyboxRef() override;
  [[nodiscard]] bool& showGridRef() override;
  [[nodiscard]] bool& showDebugObjectsRef() override;
  [[nodiscard]] bool& showColliderWireframesRef() override;
  [[nodiscard]] bool& physicsSimulationRunningRef() override;
  [[nodiscard]] bool& solidGroundEnabledRef() override;
  [[nodiscard]] SkyboxSettings& skySettingsRef() override;
  [[nodiscard]] ShadowSettings& shadowSettingsRef() override;
  [[nodiscard]] PostProcessPushConstants& postProcessPushRef() override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
