#include "Editor/Infrastructure/SceneSettingsAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

SceneSettingsAdapter::SceneSettingsAdapter(EngineState& engineState)
    : engineState_(engineState) {}

SkyboxSettings* SceneSettingsAdapter::getSkySettings() {
  return &engineState_.skySettingsRef();
}

ShadowSettings* SceneSettingsAdapter::getShadowSettings() {
  return &engineState_.shadowSettingsRef();
}

void SceneSettingsAdapter::reloadSkybox() {
  // Skybox reload requires Device context - this is handled by the caller
  // passing the Device through the EnvironmentLightingPort interface.
}

void SceneSettingsAdapter::resetShadowSettings() {
  // Reset shadow settings to defaults
  engineState_.shadowSettingsRef() = ShadowSettings{};
}

void SceneSettingsAdapter::changeShadowSettings(bool enableShadowCulling,
                                                 float pointLightDefaultRange,
                                                 float spotLightDefaultRange) {
  auto& shadow = engineState_.shadowSettingsRef();
  shadow.enableShadowCulling = enableShadowCulling;
  shadow.pointLightDefaultRange = pointLightDefaultRange;
  shadow.spotLightDefaultRange = spotLightDefaultRange;
}

}  // namespace engine
