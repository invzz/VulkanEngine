#include "Engine/State/RuntimeSettingsBindingService.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"

namespace engine {

    RuntimeSettingsBindingService::RuntimeSettingsBindingService(
        SceneSerializer&          serializer,
        bool&                     showSkybox,
        bool&                     showGrid,
        bool&                     showDebugObjects,
        bool&                     physicsSimulationRunning,
        SkyboxSettings&           skySettings,
        PostProcessPushConstants& postProcessPush,
        IBLSystem*                iblSystem,
        ModelRenderSystem*        modelRenderSystem,
        std::function<bool()>     getGpuProfilerEnabled,
        std::function<void(bool)> setGpuProfilerEnabled,
        bool&                     multithreadedRecordingEnabled,
        uint32_t&                 multithreadedRecordingThreads,
        int&                      debugMode)
        : serializer_(serializer),
          showSkybox_(showSkybox),
          showGrid_(showGrid),
          showDebugObjects_(showDebugObjects),
          physicsSimulationRunning_(physicsSimulationRunning),
          skySettings_(skySettings),
          postProcessPush_(postProcessPush),
          iblSystem_(iblSystem),
          modelRenderSystem_(modelRenderSystem),
          getGpuProfilerEnabled_(std::move(getGpuProfilerEnabled)),
          setGpuProfilerEnabled_(std::move(setGpuProfilerEnabled)),
          multithreadedRecordingEnabled_(multithreadedRecordingEnabled),
          multithreadedRecordingThreads_(multithreadedRecordingThreads),
          debugMode_(debugMode) {
    }

    void RuntimeSettingsBindingService::applyBindings() {
        serializer_.setRuntimeSettingsBindings(RuntimeSettingsBindings{
            .showSkybox                    = &showSkybox_,
            .showGrid                      = &showGrid_,
            .showDebugObjects              = &showDebugObjects_,
            .physicsSimulationRunning      = &physicsSimulationRunning_,
            .skySettings                   = &skySettings_,
            .postProcessPush               = &postProcessPush_,
            .iblSystem                     = iblSystem_,
            .modelRenderSystem             = modelRenderSystem_,
            .getGpuProfilerEnabled         = getGpuProfilerEnabled_,
            .setGpuProfilerEnabled         = setGpuProfilerEnabled_,
            .multithreadedRecordingEnabled = &multithreadedRecordingEnabled_,
            .multithreadedRecordingThreads = &multithreadedRecordingThreads_,
            .debugMode                     = &debugMode_,
        });
    }

}  // namespace engine
