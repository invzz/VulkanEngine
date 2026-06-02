#pragma once

namespace engine {

class IEnvironmentLightingPort {
 public:
  virtual ~IEnvironmentLightingPort() = default;

  virtual void syncEnvironmentLighting(bool showSkyboxEnabled) = 0;
};

}  // namespace engine