#include "Engine/Application/UseCases/SceneEntityManagementUseCase.hpp"

#include <iostream>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

SceneEntityManagementUseCase::SceneEntityManagementUseCase(ISceneEntityPort& sceneEntity)
    : sceneEntity_(sceneEntity) {}

entt::entity SceneEntityManagementUseCase::addCamera(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<CameraComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Camera" : name);
  return entity;
}

entt::entity SceneEntityManagementUseCase::addDirectionalLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<DirectionalLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Directional Light" : name);
  return entity;
}

entt::entity SceneEntityManagementUseCase::addPointLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<PointLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Point Light" : name);
  return entity;
}

entt::entity SceneEntityManagementUseCase::addSpotLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<SpotLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Spot Light" : name);
  return entity;
}

entt::entity SceneEntityManagementUseCase::addModel(const std::string& name, const std::string& modelPath) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Model" : name);
  (void)modelPath;
  return entity;
}

bool SceneEntityManagementUseCase::deleteEntity(entt::entity entity) {
  if (!sceneEntity_.isValid(entity)) {
    return false;
  }
  sceneEntity_.deleteEntity(entity);
  return true;
}

bool SceneEntityManagementUseCase::isValid(entt::entity entity) const {
  return sceneEntity_.isValid(entity);
}

Scene* SceneEntityManagementUseCase::scene() const {
  return sceneEntity_.scene();
}

}  // namespace engine
