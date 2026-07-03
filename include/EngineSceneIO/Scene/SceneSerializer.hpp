#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENESERIALIZER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENESERIALIZER_HPP

#include <functional>
#include <string>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    class IBLSystem;
    class ModelRenderSystem;

    struct RuntimeSettingsBindings {
        bool*                     showSkybox               = nullptr;
        bool*                     showGrid                 = nullptr;
        bool*                     showDebugObjects         = nullptr;
        bool*                     physicsSimulationRunning = nullptr;
        SkyboxSettings*           skySettings              = nullptr;
        PostProcessPushConstants* postProcessPush          = nullptr;
        IBLSystem*                iblSystem                = nullptr;
        ModelRenderSystem*        modelRenderSystem        = nullptr;
        std::function<bool()>     getGpuProfilerEnabled;
        std::function<void(bool)> setGpuProfilerEnabled;
        bool*                     multithreadedRecordingEnabled = nullptr;
        uint32_t*                 multithreadedRecordingThreads = nullptr;
        int*                      debugMode                     = nullptr;
        bool*                     viewGizmoOrbitSelected        = nullptr;
    };

    class SceneSerializer {
       public:
        SceneSerializer(Scene& scene, ResourceManager& resourceManager);

        void setRuntimeSettingsBindings(const RuntimeSettingsBindings& bindings);

        void serialize(const std::string& filepath);
        bool deserialize(const std::string& filepath);

       private:
        Scene&                  scene;
        ResourceManager&        resourceManager;
        RuntimeSettingsBindings settingsBindings_{};
    };

}  // namespace engine

#endif
