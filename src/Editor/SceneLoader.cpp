#include "Editor/SceneLoader.hpp"

#include <cstddef>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
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

namespace {
std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool shouldAutoCreateStaticCollider(const std::string& modelPath, const std::string& name) {
  const std::string loweredPath = toLower(modelPath);
  const std::string loweredName = toLower(name);
  const std::string combined = loweredPath + " " + loweredName;

  static const std::vector<std::string> tokens = {
      "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};

  for (const auto& token : tokens) {
    if (combined.find(token) != std::string::npos) {
      return true;
    }
  }
  return false;
}
}  // namespace

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

  if (shouldAutoCreateStaticCollider(modelPath, "LoadedModel")) {
    auto& rigidBody = scene.getRegistry().emplace<RigidBodyComponent>(entity);
    rigidBody.isStatic = true;
    rigidBody.mode = RigidBodyComponent::PhysicsMode::Static;
    rigidBody.useGravity = false;

    auto& collider = scene.getRegistry().emplace<ColliderComponent>(entity);
    collider.shape = ColliderComponent::ShapeType::Mesh;
    collider.isTrigger = false;
  }

  auto& transform = scene.getRegistry().get<TransformComponent>(entity);
  transform.scale = {1.0f, 1.f, 1.0f};
  transform.translation = {0.0f, 0.0f, 0.0f};
}

}  // namespace engine
