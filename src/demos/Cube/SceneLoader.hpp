#pragma once

#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

  class SceneLoader
  {
  public:
    static void loadScene(Device& device, Scene& scene, ResourceManager& resourceManager);

  private:
    static void createFromFile(Device& device, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath);
    static void createApple(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createSpaceShip(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createLights(Scene& scene, float radius = 2.0f);
    static void createFloor(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createDragonGrid(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createBmw(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createCylinderEngine(Device& device, Scene& scene, ResourceManager& resourceManager);
    static void createDemoObject(Device& device, Scene& scene, ResourceManager& resourceManager);
  };

} // namespace engine
