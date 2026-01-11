#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENESERIALIZER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENESERIALIZER_HPP

#include <string>

#include "ModelLib/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

  class SceneSerializer
  {
  public:
    SceneSerializer(Scene& scene, ResourceManager& resourceManager);

    void serialize(const std::string& filepath);
    bool deserialize(const std::string& filepath);

  private:
    Scene&           scene;
    ResourceManager& resourceManager;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENESERIALIZER_HPP
