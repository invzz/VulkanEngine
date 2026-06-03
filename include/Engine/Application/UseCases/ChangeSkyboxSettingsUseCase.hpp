#pragma once

#include "Engine/Application/Ports/ISceneSettingsPort.hpp"

namespace engine {

// Use case for changing skybox settings.
class ChangeSkyboxSettingsUseCase {
 public:
  explicit ChangeSkyboxSettingsUseCase(ISceneSettingsPort& sceneSettings);

  // Toggle debug cubemap faces display.
  void execute(bool debugCubemapFaces);

  // Reload the skybox from the current path.
  void reloadSkybox();

 private:
  ISceneSettingsPort& sceneSettings_;
};

}  // namespace engine
