#include "Engine/Application/UseCases/ChangeSkyboxSettingsUseCase.hpp"

#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

namespace engine {

ChangeSkyboxSettingsUseCase::ChangeSkyboxSettingsUseCase(ISceneSettingsPort& sceneSettings)
    : sceneSettings_(sceneSettings) {}

void ChangeSkyboxSettingsUseCase::execute(bool debugCubemapFaces) {
  auto* skySettings = sceneSettings_.getSkySettings();
  if (skySettings != nullptr) {
    skySettings->debugCubemapFaces = debugCubemapFaces;
  }
}

void ChangeSkyboxSettingsUseCase::reloadSkybox() {
  sceneSettings_.reloadSkybox();
}

}  // namespace engine
