#include "Engine/Application/UseCases/SetActiveCameraUseCase.hpp"

namespace engine {

SetActiveCameraUseCase::SetActiveCameraUseCase(ICameraPort& cameraPort)
    : cameraPort_(cameraPort) {}

void SetActiveCameraUseCase::execute(entt::entity cameraEntity, SceneRuntimeState& runtimeState) {
  cameraPort_.setActiveCamera(cameraEntity, runtimeState);
}

}  // namespace engine
