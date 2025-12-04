#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>

namespace engine {

  class GameObject;

  class Component
  {
  public:
    virtual ~Component() = default;

    GameObject* getOwner() const { return owner; }
    void        setOwner(GameObject* newOwner) { owner = newOwner; }

  protected:
    GameObject* owner = nullptr;
  };

} // namespace engine
