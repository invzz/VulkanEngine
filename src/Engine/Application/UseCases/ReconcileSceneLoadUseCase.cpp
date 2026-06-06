#include "Engine/Application/UseCases/ReconcileSceneLoadUseCase.hpp"

#include "Engine/Scene/components/CameraComponent.hpp"

namespace engine {

    ReconcileSceneLoadUseCase::ReconcileSceneLoadUseCase(Scene& scene)
        : scene_(scene) {}

    void ReconcileSceneLoadUseCase::execute(SceneRuntimeState& runtimeState) const {
        if (!runtimeState.pendingUpdateCameraAfterSceneLoad) {
            return;
        }

        runtimeState.pendingUpdateCameraAfterSceneLoad = false;

        runtimeState.cameraEntity = entt::null;
        auto const& registry      = scene_.getRegistry();
        auto        view          = registry.view<engine::CameraComponent>();
        for (auto entity : view) {
            runtimeState.cameraEntity = entity;
            break;
        }
    }

}  // namespace engine