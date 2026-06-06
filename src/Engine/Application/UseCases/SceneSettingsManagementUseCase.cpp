#include "Engine/Application/UseCases/SceneSettingsManagementUseCase.hpp"

#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

namespace engine {

    SceneSettingsManagementUseCase::SceneSettingsManagementUseCase(
        ISceneSettingsPort&  sceneSettings,
        IPhysicsRuntimePort& physicsRuntime)
        : sceneSettings_(sceneSettings),
          physicsRuntime_(physicsRuntime) {}

    // ---- Skybox settings ----

    void SceneSettingsManagementUseCase::setDebugCubemapFaces(bool debugCubemapFaces) {
        auto* skySettings = sceneSettings_.getSkySettings();
        if (skySettings != nullptr) {
            skySettings->debugCubemapFaces = debugCubemapFaces;
        }
    }

    void SceneSettingsManagementUseCase::reloadSkybox() {
        sceneSettings_.reloadSkybox();
    }

    SkyboxSettings* SceneSettingsManagementUseCase::getSkySettings() const {
        return sceneSettings_.getSkySettings();
    }

    // ---- Shadow settings ----

    void SceneSettingsManagementUseCase::setShadowSettings(bool enableShadowCulling,
        float                                                   pointLightDefaultRange,
        float                                                   spotLightDefaultRange) {
        auto* shadowSettings = sceneSettings_.getShadowSettings();
        if (shadowSettings != nullptr) {
            shadowSettings->enableShadowCulling    = enableShadowCulling;
            shadowSettings->pointLightDefaultRange = pointLightDefaultRange;
            shadowSettings->spotLightDefaultRange  = spotLightDefaultRange;
        }
    }

    void SceneSettingsManagementUseCase::resetShadowSettings() {
        sceneSettings_.resetShadowSettings();
    }

    ShadowSettings* SceneSettingsManagementUseCase::getShadowSettings() const {
        return sceneSettings_.getShadowSettings();
    }

    void SceneSettingsManagementUseCase::changeShadowSettings(bool enableShadowCulling,
        float                                                      pointLightDefaultRange,
        float                                                      spotLightDefaultRange) {
        sceneSettings_.changeShadowSettings(enableShadowCulling, pointLightDefaultRange, spotLightDefaultRange);
    }

    // ---- Physics settings ----

    void SceneSettingsManagementUseCase::togglePhysicsSimulation(bool& simulationRunningRef) {
        simulationRunningRef = !simulationRunningRef;
    }

    void SceneSettingsManagementUseCase::setGroundEnabled(bool enabled) {
        physicsRuntime_.setGroundEnabled(enabled);
    }

    bool SceneSettingsManagementUseCase::isPhysicsRunning() const {
        return physicsRuntime_.physicsSimulationRunningRef();
    }

}  // namespace engine
