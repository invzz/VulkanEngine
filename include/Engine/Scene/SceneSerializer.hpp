#pragma once

#include <string>

#include "Engine/Resources/ResourceManager.hpp"
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
