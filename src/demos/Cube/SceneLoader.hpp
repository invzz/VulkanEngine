#pragma once

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"

namespace engine {

  class SceneLoader
  {
  public:
    static void loadScene(Device& device, GameObject::Map& gameObjects);

  private:
    static void createApple(Device& device, GameObject::Map& gameObjects);
    static void createSpaceShip(Device& device, GameObject::Map& gameObjects);
    static void createLights(GameObject::Map& gameObjects, float radius = 2.0f);
    static void createFloor(Device& device, GameObject::Map& gameObjects);
    static void createDragonGrid(Device& device, GameObject::Map& gameObjects);
    static void createBmw(Device& device, GameObject::Map& gameObjects);
  };

} // namespace engine
