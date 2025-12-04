#pragma once

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/ResourceManager.hpp"

namespace engine {

  class SceneLoader
  {
  public:
    static void loadScene(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);

  private:
    static void createFromFile(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager, const std::string& modelPath);
    static void createApple(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createSpaceShip(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createLights(GameObjectManager& objectManager, float radius = 2.0f);
    static void createFloor(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createDragonGrid(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createBmw(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createCylinderEngine(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
    static void createDemoObject(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager);
  };

} // namespace engine
