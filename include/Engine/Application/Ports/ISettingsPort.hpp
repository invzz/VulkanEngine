#pragma once

#include <string>

namespace engine {

    class SkyboxSettings;
    class PostProcessPushConstants;
    class RenderingStateService;
    class ResourceManager;
    class ModelRenderSystem;

    class ISettingsPort {
       public:
        virtual ~ISettingsPort() = default;

        virtual void setShowSkybox(bool enabled) = 0;
        virtual bool showSkybox() const          = 0;

        virtual void setShowGrid(bool enabled) = 0;
        virtual bool showGrid() const          = 0;

        virtual void setShowDebugObjects(bool enabled) = 0;
        virtual bool showDebugObjects() const          = 0;

        virtual void           setSkySettings(const SkyboxSettings& settings) = 0;
        virtual SkyboxSettings skySettings() const                            = 0;

        virtual void                     setPostProcessSettings(const PostProcessPushConstants& settings) = 0;
        virtual PostProcessPushConstants postProcessSettings() const                                      = 0;

        // For ImGui bindings that need mutable pointers
        [[nodiscard]] virtual bool*                     showSkyboxPtr()       = 0;
        [[nodiscard]] virtual bool*                     showGridPtr()         = 0;
        [[nodiscard]] virtual bool*                     showDebugObjectsPtr() = 0;
        [[nodiscard]] virtual SkyboxSettings*           skySettingsPtr()      = 0;
        [[nodiscard]] virtual PostProcessPushConstants* postProcessPush()     = 0;

        // Infrastructure accessors needed by UI panels
        [[nodiscard]] virtual class RenderingStateService renderingService()  = 0;
        [[nodiscard]] virtual class ResourceManager*      resourceManager()   = 0;
        [[nodiscard]] virtual class ModelRenderSystem*    modelRenderSystem() = 0;
    };

}  // namespace engine
