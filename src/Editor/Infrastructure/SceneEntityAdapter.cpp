#include "Editor/Infrastructure/SceneEntityAdapter.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

    SceneEntityAdapter::SceneEntityAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    entt::entity SceneEntityAdapter::createEntity() {
        return engineState_.sceneRef().createEntity();
    }

    void SceneEntityAdapter::deleteEntity(entt::entity entity) {
        engineState_.sceneRef().destroyEntity(entity);
    }

    bool SceneEntityAdapter::isValid(entt::entity entity) const {
        return engineState_.sceneRef().getRegistry().valid(entity);
    }

    Scene* SceneEntityAdapter::scene() {
        return &engineState_.sceneRef();
    }

}  // namespace engine
