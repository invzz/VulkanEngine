#pragma once

#include <string>

#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/GameObjectManager.hpp"

namespace engine {

  class SceneSerializer
  {
  public:
    SceneSerializer(GameObjectManager& manager, ResourceManager& resourceManager);

    void serialize(const std::string& filepath);
    bool deserialize(const std::string& filepath);

  private:
    GameObjectManager& manager;
    ResourceManager&   resourceManager;
  };

} // namespace engine
