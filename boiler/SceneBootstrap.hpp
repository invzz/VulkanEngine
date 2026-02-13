#ifndef SCENE_BOOTSTRAP_HPP
#define SCENE_BOOTSTRAP_HPP

#include "Engine/Scene/Scene.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {
class SceneBootstrap {
 public:
  SceneBootstrap(Scene& scene, ResourceManager& rm);

  void createDefaultCamera();
  bool loadScene(const std::string& path);
  void saveScene(const std::string& path);

  [[nodiscard]] entt::entity getPrimaryCamera() const;

 private:
  Scene& scene;
  SceneSerializer serializer;
  entt::entity cameraEntity{entt::null};
};
}  // namespace engine
#endif  // SCENE_BOOTSTRAP_HPP