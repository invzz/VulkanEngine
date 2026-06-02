#ifndef VULKANENGINE_INCLUDE_ENGINE_STATE_RUNTIMESETTINGSBINDINGSERVICE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_STATE_RUNTIMESETTINGSBINDINGSERVICE_HPP

#include <cstdint>
#include <functional>

namespace engine {

class SceneSerializer;
class IBLSystem;
class ModelRenderSystem;
class SkyboxSettings;
class PostProcessPushConstants;

// Application-level service that owns the logic of building and applying
// runtime settings bindings to a SceneSerializer.  Delivery (app.cpp) no
// longer constructs RuntimeSettingsBindings directly.
class RuntimeSettingsBindingService {
 public:
  // Construct with the serializer and all runtime state references the
  // application layer owns.  No delivery-layer types leak through here.
  RuntimeSettingsBindingService(
      SceneSerializer& serializer,
      bool& showSkybox,
      bool& showGrid,
      bool& showDebugObjects,
      bool& physicsSimulationRunning,
      SkyboxSettings& skySettings,
      PostProcessPushConstants& postProcessPush,
      IBLSystem* iblSystem,
      ModelRenderSystem* modelRenderSystem,
      std::function<bool()> getGpuProfilerEnabled,
      std::function<void(bool)> setGpuProfilerEnabled,
      bool& multithreadedRecordingEnabled,
      uint32_t& multithreadedRecordingThreads,
      int& debugMode);

  // Apply the bindings to the serializer.  Call once during engine
  // initialization after all referenced state objects are alive.
  void applyBindings();

 private:
  SceneSerializer& serializer_;
  bool& showSkybox_;
  bool& showGrid_;
  bool& showDebugObjects_;
  bool& physicsSimulationRunning_;
  SkyboxSettings& skySettings_;
  PostProcessPushConstants& postProcessPush_;
  IBLSystem* iblSystem_;
  ModelRenderSystem* modelRenderSystem_;
  std::function<bool()> getGpuProfilerEnabled_;
  std::function<void(bool)> setGpuProfilerEnabled_;
  bool& multithreadedRecordingEnabled_;
  uint32_t& multithreadedRecordingThreads_;
  int& debugMode_;
};

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_STATE_RUNTIMESETTINGSBINDINGSERVICE_HPP
