#include "Editor/Infrastructure/CameraAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

CameraAdapter::CameraAdapter(EngineState& engineState)
    : engineState_(engineState) {}

void CameraAdapter::setActiveCamera(entt::entity cameraEntity, SceneRuntimeState& runtimeState) {
  // Set in the runtime state view (references EngineState::Impl::cameraEntity)
  runtimeState.cameraEntity = cameraEntity;
}

entt::entity CameraAdapter::getActiveCamera(const SceneRuntimeState& runtimeState) const {
  return runtimeState.cameraEntity;
}

}  // namespace engine
