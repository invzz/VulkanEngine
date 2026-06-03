#include "Engine/Application/UseCases/ChangeShadowSettingsUseCase.hpp"

#include "Engine/Systems/ShadowSystem.hpp"

namespace engine {

ChangeShadowSettingsUseCase::ChangeShadowSettingsUseCase(ISceneSettingsPort& sceneSettings)
    : sceneSettings_(sceneSettings) {}

void ChangeShadowSettingsUseCase::execute(bool enableShadowCulling, float pointLightDefaultRange, float spotLightDefaultRange) {
  auto* shadowSettings = sceneSettings_.getShadowSettings();
  if (shadowSettings != nullptr) {
    shadowSettings->enableShadowCulling = enableShadowCulling;
    shadowSettings->pointLightDefaultRange = pointLightDefaultRange;
    shadowSettings->spotLightDefaultRange = spotLightDefaultRange;
  }
}

void ChangeShadowSettingsUseCase::resetDefaults() {
  sceneSettings_.resetShadowSettings();
}

}  // namespace engine
