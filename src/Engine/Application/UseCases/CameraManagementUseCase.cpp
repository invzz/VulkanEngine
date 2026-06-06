#include "Engine/Application/UseCases/CameraManagementUseCase.hpp"

namespace engine {

    CameraManagementUseCase::CameraManagementUseCase(ICameraPort& cameraPort)
        : cameraPort_(cameraPort) {}

    void CameraManagementUseCase::setActiveCamera(entt::entity cameraEntity, SceneRuntimeState& runtimeState) {
        cameraPort_.setActiveCamera(cameraEntity, runtimeState);
    }

    entt::entity CameraManagementUseCase::getActiveCamera(const SceneRuntimeState& runtimeState) const {
        return cameraPort_.getActiveCamera(runtimeState);
    }

}  // namespace engine
