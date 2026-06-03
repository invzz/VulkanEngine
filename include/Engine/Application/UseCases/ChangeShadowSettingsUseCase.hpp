#pragma once

#include "Engine/Application/Ports/ISceneSettingsPort.hpp"

namespace engine {

// Use case for changing shadow settings.
class ChangeShadowSettingsUseCase {
 public:
  explicit ChangeShadowSettingsUseCase(ISceneSettingsPort& sceneSettings);

  // Update shadow settings with new values.
  void execute(bool enableShadowCulling, float pointLightDefaultRange, float spotLightDefaultRange);

  // Reset shadow settings to defaults.
  void resetDefaults();

 private:
  ISceneSettingsPort& sceneSettings_;
};

}  // namespace engine
