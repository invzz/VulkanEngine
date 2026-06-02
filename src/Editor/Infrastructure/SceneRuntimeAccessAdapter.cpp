#include "Editor/Infrastructure/SceneRuntimeAccessAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

SceneRuntimeAccessAdapter::SceneRuntimeAccessAdapter(EngineState& engineState)
    : engineState_(engineState) {}

Scene* SceneRuntimeAccessAdapter::scene() {
  return engineState_.sceneRuntimeService().view().scene;
}

bool* SceneRuntimeAccessAdapter::physicsSimulationRunning() {
  return &engineState_.physicsSimulationRunningRef();
}

bool* SceneRuntimeAccessAdapter::showColliderWireframes() {
  return &engineState_.showColliderWireframesRef();
}

bool* SceneRuntimeAccessAdapter::solidGroundEnabled() {
  return &engineState_.solidGroundEnabledRef();
}

SkyboxSettings* SceneRuntimeAccessAdapter::skySettings() {
  return &engineState_.skySettingsRef();
}

ShadowSettings* SceneRuntimeAccessAdapter::shadowSettings() {
  return &engineState_.shadowSettingsRef();
}

PostProcessPushConstants* SceneRuntimeAccessAdapter::postProcessPush() {
  return &engineState_.postProcessPushRef();
}

}  // namespace engine
