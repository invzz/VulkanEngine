#pragma once

#include <memory>
#include <vector>

#include "Engine/Scene/AnimationController.hpp"
#include "Engine/Scene/GameObject.hpp"

namespace engine {

  // Organizes game objects by type for efficient iteration
  class GameObjectManager
  {
  public:
    GameObjectManager() = default;

    // Add objects to appropriate vectors based on their components
    void                      addObject(GameObject&& obj);
    void                      removeObject(GameObject::id_t id); // Access by type
    void                      clear();
    std::vector<GameObject*>& getPBRObjects() { return pbrObjects_; }
    std::vector<GameObject*>& getPointLights() { return pointLights_; }
    std::vector<GameObject*>& getDirectionalLights() { return directionalLights_; }
    std::vector<GameObject*>& getSpotLights() { return spotLights_; }
    std::vector<GameObject*>& getLODObjects() { return lodObjects_; }

    // Access all objects
    GameObject::Map& getAllObjects() { return allObjects_; }
    GameObject*      getObject(GameObject::id_t id);

    // Get count by type
    size_t getPBRObjectCount() const { return pbrObjects_.size(); }
    size_t getPointLightCount() const { return pointLights_.size(); }
    size_t getDirectionalLightCount() const { return directionalLights_.size(); }
    size_t getSpotLightCount() const { return spotLights_.size(); }

  private:
    GameObject::Map allObjects_;

    // Pointers to objects organized by type
    std::vector<GameObject*> pbrObjects_;
    std::vector<GameObject*> pointLights_;
    std::vector<GameObject*> directionalLights_;
    std::vector<GameObject*> spotLights_;
    std::vector<GameObject*> lodObjects_;

    void categorizeObject(GameObject* obj);
    void removeFromCategory(GameObject* obj);
  };

} // namespace engine
