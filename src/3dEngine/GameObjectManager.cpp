#include "3dEngine/GameObjectManager.hpp"

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
    if (obj->pbrMaterial)
    {
      pbrObjects_.push_back(obj);
    }

    if (obj->pointLight)
    {
      pointLights_.push_back(obj);
    }

    if (obj->directionalLight)
    {
      directionalLights_.push_back(obj);
    }

    if (obj->spotLight)
    {
      spotLights_.push_back(obj);
    }

    if (obj->lodComponent)
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

    if (obj->pbrMaterial)
    {
      removeFromVector(pbrObjects_);
    }

    if (obj->pointLight)
    {
      removeFromVector(pointLights_);
    }

    if (obj->directionalLight)
    {
      removeFromVector(directionalLights_);
    }

    if (obj->spotLight)
    {
      removeFromVector(spotLights_);
    }

    if (obj->lodComponent)
    {
      removeFromVector(lodObjects_);
    }
  }

} // namespace engine
