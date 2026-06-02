#pragma once

#include "Engine/Application/Ports/IEnvironmentLightingPort.hpp"

namespace engine {

class SyncEnvironmentLightingUseCase {
 public:
  explicit SyncEnvironmentLightingUseCase(IEnvironmentLightingPort& environmentLighting);

  void execute(bool showSkyboxEnabled) const;

 private:
  IEnvironmentLightingPort& environmentLighting_;
};

}  // namespace engine