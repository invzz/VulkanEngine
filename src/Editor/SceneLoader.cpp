#include "Editor/SceneLoader.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

namespace engine {

void SceneLoader::loadScene(Device& device, Scene& scene, ResourceManager& resourceManager) {
  if (!scene.getRegistry().storage<entt::entity>().empty()) {
    return;
  }
}

void SceneLoader::createFromFile(Device& /*device*/, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath) {
  if (!scene.getRegistry().storage<entt::entity>().empty()) {
    return;
  }

  auto modelPtr = resourceManager.loadModel(modelPath, true, true, true);

  auto entity = scene.createEntity();
  scene.getRegistry().emplace<TransformComponent>(entity);
  scene.getRegistry().emplace<ModelComponent>(entity, std::move(modelPtr));
  scene.getRegistry().emplace<NameComponent>(entity, "LoadedModel");

  auto& transform = scene.getRegistry().get<TransformComponent>(entity);
  transform.scale = {1.0f, 1.f, 1.0f};
  transform.translation = {0.0f, 0.0f, 0.0f};
}

}  // namespace engine
