#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENE_HPP

#include <entt/entt.hpp>

namespace engine {

class Scene {
 public:
  Scene() = default;
  ~Scene() = default;

  entt::entity createEntity() {
    return registry.create();
  }
  void destroyEntity(entt::entity entity) {
    registry.destroy(entity);
  }

  entt::registry& getRegistry() {
    return registry;
  }
  [[nodiscard]] const entt::registry& getRegistry() const {
    return registry;
  }

 private:
  entt::registry registry;
};

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENE_HPP
