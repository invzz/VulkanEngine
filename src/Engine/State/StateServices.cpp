#include "Engine/State/StateServices.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

RenderingStateService::RenderingStateService(EngineState& engineState)
    : engineState_(engineState) {}

RenderingStateView RenderingStateService::view() const {
  return engineState_.renderingState();
}

SceneRuntimeService::SceneRuntimeService(EngineState& engineState)
    : engineState_(engineState) {}

SceneRuntimeStateView SceneRuntimeService::view() const {
  return engineState_.sceneState();
}

InputStateService::InputStateService(EngineState& engineState)
    : engineState_(engineState) {}

InputStateView InputStateService::view() const {
  return engineState_.inputState();
}

ResourceStateService::ResourceStateService(EngineState& engineState)
    : engineState_(engineState) {}

ResourceStateView ResourceStateService::view() const {
  return engineState_.resourceState();
}

}  // namespace engine