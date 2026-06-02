#pragma once

#include <cstdint>

#include "Engine/Application/Ports/IEnvironmentLightingPort.hpp"

namespace engine {

class Device;
class EngineState;

class EnvironmentLightingAdapter final : public IEnvironmentLightingPort {
 public:
  EnvironmentLightingAdapter(Device& device, EngineState& engineState);

  void syncEnvironmentLighting(bool showSkyboxEnabled) override;

 private:
  Device& device_;
  EngineState& engineState_;
  uint64_t iblGenerationCounter_ = 0;
};

}  // namespace engine