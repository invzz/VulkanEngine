#pragma once

#include <glm/glm.hpp>

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"
#include "Engine/Application/Ports/ISceneSettingsPort.hpp"

namespace engine {

    class SkyboxSettings;
    class ShadowSettings;

    // Use case for scene settings management (skybox, shadows, physics).
    // Aggregates ChangeSkyboxSettingsUseCase, ChangeShadowSettingsUseCase,
    // and TogglePhysicsSimulationUseCase operations.
    class SceneSettingsManagementUseCase {
       public:
        explicit SceneSettingsManagementUseCase(
            ISceneSettingsPort&  sceneSettings,
            IPhysicsRuntimePort& physicsRuntime);

        // ---- Skybox settings ----

        // Toggle debug cubemap faces display.
        void setDebugCubemapFaces(bool debugCubemapFaces);

        // Reload the skybox from the current path.
        void reloadSkybox();

        // Get current sky settings.
        [[nodiscard]] SkyboxSettings* getSkySettings() const;

        // ---- Shadow settings ----

        // Update shadow settings with new values.
        void setShadowSettings(bool enableShadowCulling,
            float                   pointLightDefaultRange,
            float                   spotLightDefaultRange);

        // Reset shadow settings to defaults.
        void resetShadowSettings();

        // Get current shadow settings.
        [[nodiscard]] ShadowSettings* getShadowSettings() const;

        // Change shadow-related settings.
        void changeShadowSettings(bool enableShadowCulling,
            float                      pointLightDefaultRange,
            float                      spotLightDefaultRange);

        // ---- Physics settings ----

        // Toggle physics simulation running state.
        void togglePhysicsSimulation(bool& simulationRunningRef);

        // Set ground enabled state.
        void setGroundEnabled(bool enabled);

        // Get current simulation running state.
        [[nodiscard]] bool isPhysicsRunning() const;

       private:
        ISceneSettingsPort&  sceneSettings_;
        IPhysicsRuntimePort& physicsRuntime_;
    };

}  // namespace engine
