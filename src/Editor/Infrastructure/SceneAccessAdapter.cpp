#include "Editor/Infrastructure/SceneAccessAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

    SceneAccessAdapter::SceneAccessAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    Scene* SceneAccessAdapter::scene() {
        return engineState_.sceneRuntimeService().view().scene;
    }

    Skybox* SceneAccessAdapter::skybox() {
        return engineState_.sceneRuntimeService().view().skybox;
    }

    SkyboxSettings* SceneAccessAdapter::skySettings() {
        return engineState_.sceneRuntimeService().view().skySettings;
    }

    ShadowSettings* SceneAccessAdapter::shadowSettings() {
        return engineState_.sceneRuntimeService().view().shadowSettings;
    }

}  // namespace engine
