#pragma once

#include <string>

#include "GameObjectManager.hpp"
#include "ResourceManager.hpp"

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
