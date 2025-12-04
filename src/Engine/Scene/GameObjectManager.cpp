#include "Engine/Scene/GameObjectManager.hpp"

#include <algorithm>

namespace engine {

  void GameObjectManager::addObject(GameObject&& obj)
  {
    auto id     = obj.getId();
    auto result = allObjects_.emplace(id, std::move(obj));
    categorizeObject(&result.first->second);
  }

  void GameObjectManager::removeObject(GameObject::id_t id)
  {
    auto it = allObjects_.find(id);
    if (it == allObjects_.end()) return;

    removeFromCategory(&it->second);
    allObjects_.erase(it);
  }

  void GameObjectManager::clear()
  {
    allObjects_.clear();
    pbrObjects_.clear();
    pointLights_.clear();
    directionalLights_.clear();
    spotLights_.clear();
    lodObjects_.clear();
  }

  GameObject* GameObjectManager::getObject(GameObject::id_t id)
  {
    auto it = allObjects_.find(id);
    return (it != allObjects_.end()) ? &it->second : nullptr;
  }

  void GameObjectManager::categorizeObject(GameObject* obj)
  {
    if (!obj) return;

    // Add to appropriate type-specific vectors
    if (obj->getComponent<PBRMaterial>())
    {
      pbrObjects_.push_back(obj);
    }

    if (obj->getComponent<PointLightComponent>())
    {
      pointLights_.push_back(obj);
    }

    if (obj->getComponent<DirectionalLightComponent>())
    {
      directionalLights_.push_back(obj);
    }

    if (obj->getComponent<SpotLightComponent>())
    {
      spotLights_.push_back(obj);
    }

    if (obj->getComponent<LODComponent>())
    {
      lodObjects_.push_back(obj);
    }
  }

  void GameObjectManager::removeFromCategory(GameObject* obj)
  {
    if (!obj) return;

    auto removeFromVector = [obj](std::vector<GameObject*>& vec) {
      auto it = std::find(vec.begin(), vec.end(), obj);
      if (it != vec.end())
      {
        vec.erase(it);
      }
    };

    if (obj->getComponent<PBRMaterial>())
    {
      removeFromVector(pbrObjects_);
    }

    if (obj->getComponent<PointLightComponent>())
    {
      removeFromVector(pointLights_);
    }

    if (obj->getComponent<DirectionalLightComponent>())
    {
      removeFromVector(directionalLights_);
    }

    if (obj->getComponent<SpotLightComponent>())
    {
      removeFromVector(spotLights_);
    }

    if (obj->getComponent<LODComponent>())
    {
      removeFromVector(lodObjects_);
    }
  }

} // namespace engine
