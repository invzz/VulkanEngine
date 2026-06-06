#pragma once

namespace engine {

    class SkyboxSettings;
    class ShadowSettings;

    // Port for scene settings (skybox, shadows) without knowing EngineState internals.
    class ISceneSettingsPort {
       public:
        virtual ~ISceneSettingsPort()                                                                                                                   = default;
        [[nodiscard]] virtual SkyboxSettings* getSkySettings()                                                                                          = 0;
        [[nodiscard]] virtual ShadowSettings* getShadowSettings()                                                                                       = 0;
        virtual void                          changeShadowSettings(bool enableShadowCulling, float pointLightDefaultRange, float spotLightDefaultRange) = 0;
        virtual void                          reloadSkybox()                                                                                            = 0;
        virtual void                          resetShadowSettings()                                                                                     = 0;
    };

}  // namespace engine
