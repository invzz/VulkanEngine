#include "Engine/Application/UseCases/LoadSceneUseCase.hpp"

#include <iostream>

#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

    LoadSceneUseCase::LoadSceneUseCase(Scene& scene, IScenePersistencePort& scenePersistence, IPhysicsRuntimePort* physicsRuntime)
        : scene_(scene), scenePersistence_(scenePersistence), physicsRuntime_(physicsRuntime) {}

    bool LoadSceneUseCase::execute(const std::string& path, SceneRuntimeState& runtimeState) const {
        if (physicsRuntime_ != nullptr) {
            physicsRuntime_->clearSceneBodies();
            physicsRuntime_->setGroundEnabled(runtimeState.solidGroundEnabled);
        }

        if (!scenePersistence_.loadScene(path)) {
            return false;
        }

        // Physics must always start paused after a scene load.
        runtimeState.physicsSimulationRunning = false;

        // Reset transient selection state to avoid dangling entity references.
        runtimeState.selectedEntity   = entt::null;
        runtimeState.selectedObjectId = 0;
        runtimeState.cameraEntity     = entt::null;

        ensureCameraExists(runtimeState.cameraEntity);

        runtimeState.pendingUpdateCameraAfterSceneLoad = true;
        return true;
    }

    void LoadSceneUseCase::ensureCameraExists(entt::entity& cameraEntity) const {
        auto const& registry = scene_.getRegistry();
        auto        view     = registry.view<engine::CameraComponent>();
        for (auto entity : view) {
            cameraEntity = entity;
            return;
        }

        std::cout << "[LoadSceneUseCase] Creating default camera for the scene" << '\n';
        cameraEntity = scene_.createEntity();
        scene_.getRegistry().emplace<TransformComponent>(cameraEntity);
        scene_.getRegistry().emplace<NameComponent>(cameraEntity, "Camera");
        scene_.getRegistry().emplace<CameraComponent>(cameraEntity);
    }

}  // namespace engine
