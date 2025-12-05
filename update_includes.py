import os

content = r"""#pragma once

#include "Scene.hpp"
#include "components/TransformComponent.hpp"

namespace engine {

  class GameObject
  {
  public:
    GameObject() = default;
    GameObject(entt::entity handle, Scene* scene) : entityHandle(handle), scene(scene) {}

    template<typename T, typename... Args>
    T& addComponent(Args&&... args)
    {
      return scene->getRegistry().emplace<T>(entityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& getComponent()
    {
      return scene->getRegistry().get<T>(entityHandle);
    }

    template<typename T>
    const T& getComponent() const
    {
      return scene->getRegistry().get<T>(entityHandle);
    }

    template<typename T>
    bool hasComponent() const
    {
      return scene->getRegistry().all_of<T>(entityHandle);
    }

    template<typename T>
    void removeComponent()
    {
      scene->getRegistry().remove<T>(entityHandle);
    }

    bool isValid() const
    {
      return scene && scene->getRegistry().valid(entityHandle);
    }

    operator bool() const { return isValid(); }
    
    bool operator==(const GameObject& other) const
    {
      return entityHandle == other.entityHandle && scene == other.scene;
    }

    bool operator!=(const GameObject& other) const
    {
      return !(*this == other);
    }

    entt::entity getEntity() const { return entityHandle; }

    // Convenience accessor for Transform since every object usually has one
    TransformComponent& transform()
    {
      return getComponent<TransformComponent>();
    }

    const TransformComponent& transform() const
    {
      return getComponent<TransformComponent>();
    }
    
    uint32_t getId() const { return (uint32_t)entityHandle; }

  private:
    entt::entity entityHandle{entt::null};
    Scene* scene{nullptr};
  };

} // namespace engine
"""

with open("include/Engine/Scene/GameObject.hpp", "w") as f:
    f.write(content)
