#pragma once

#include <entt/entt.hpp>

namespace engine {

  class Scene
  {
  public:
    Scene()  = default;
    ~Scene() = default;

    entt::entity createEntity() { return registry.create(); }
    void         destroyEntity(entt::entity entity) { registry.destroy(entity); }

    entt::registry&       getRegistry() { return registry; }
    const entt::registry& getRegistry() const { return registry; }

  private:
    entt::registry registry;
  };

} // namespace engine
