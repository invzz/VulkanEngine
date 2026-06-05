#include "Editor/Infrastructure/SceneManagementPortAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

SceneManagementPortAdapter::SceneManagementPortAdapter(EngineState& engineState)
    : engineState_(engineState) {}

entt::entity SceneManagementPortAdapter::createEntity() {
  return engineState_.sceneRef().createEntity();
}

void SceneManagementPortAdapter::destroyEntity(entt::entity entity) {
  engineState_.sceneRef().destroyEntity(entity);
}

void SceneManagementPortAdapter::addModel(entt::entity entity, const std::string& modelPath, const glm::mat4& transform) {
  // For now, we just add a placeholder. The actual model loading is done
  // asynchronously via ResourceManager::enqueueModelLoad.
  // This method can be extended to add a ModelComponent with a handle.
  (void)entity;
  (void)modelPath;
  (void)transform;
}

void SceneManagementPortAdapter::setSelectedEntity(entt::entity entity) {
  engineState_.selectedEntityRef() = entity;
}

entt::entity SceneManagementPortAdapter::getSelectedEntity() const {
  return engineState_.selectedEntityRef();
}

void SceneManagementPortAdapter::setCameraEntity(entt::entity entity) {
  engineState_.cameraEntityRef() = entity;
}

entt::entity SceneManagementPortAdapter::getCameraEntity() const {
  return engineState_.cameraEntityRef();
}

Scene* SceneManagementPortAdapter::scene() {
  return &engineState_.sceneRef();
}

entt::registry& SceneManagementPortAdapter::registry() {
  return engineState_.sceneRef().getRegistry();
}

ResourceManager* SceneManagementPortAdapter::resourceManager() {
  return engineState_.resourceService().view().resourceManager;
}

}  // namespace engine
