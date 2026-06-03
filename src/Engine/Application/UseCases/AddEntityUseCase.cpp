#include "Engine/Application/UseCases/AddEntityUseCase.hpp"

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

AddEntityUseCase::AddEntityUseCase(ISceneEntityPort& sceneEntity)
    : sceneEntity_(sceneEntity) {}

entt::entity AddEntityUseCase::addCamera(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<CameraComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Camera" : name);
  return entity;
}

entt::entity AddEntityUseCase::addDirectionalLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<DirectionalLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Directional Light" : name);
  return entity;
}

entt::entity AddEntityUseCase::addPointLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<PointLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Point Light" : name);
  return entity;
}

entt::entity AddEntityUseCase::addSpotLight(const std::string& name) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<SpotLightComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Spot Light" : name);
  return entity;
}

entt::entity AddEntityUseCase::addModel(const std::string& name, const std::string& modelPath) {
  auto entity = sceneEntity_.createEntity();
  auto& registry = sceneEntity_.scene()->getRegistry();
  registry.emplace<TransformComponent>(entity);
  registry.emplace<NameComponent>(entity, name.empty() ? "Model" : name);
  // ModelComponent requires a loaded model pointer - the caller must populate it
  // after the model is loaded asynchronously.
  (void)modelPath;
  return entity;
}

}  // namespace engine
