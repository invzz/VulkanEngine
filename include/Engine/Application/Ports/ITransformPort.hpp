#pragma once

#include <glm/glm.hpp>

#include "entt/entity/fwd.hpp"

namespace engine {

class Scene;

// Port for transform operations without knowing EngineState internals.
class ITransformPort {
 public:
  virtual ~ITransformPort() = default;

  // Get the current transform of an entity.
  [[nodiscard]] virtual glm::vec3 getTranslation(entt::entity entity) = 0;
  [[nodiscard]] virtual glm::vec3 getRotation(entt::entity entity) = 0;
  [[nodiscard]] virtual glm::vec3 getScale(entt::entity entity) = 0;

  // Set the transform of an entity.
  virtual void setTranslation(entt::entity entity, const glm::vec3& translation) = 0;
  virtual void setRotation(entt::entity entity, const glm::vec3& rotation) = 0;
  virtual void setScale(entt::entity entity, const glm::vec3& scale) = 0;

  // Apply a delta transform to an entity.
  virtual void applyTranslationDelta(entt::entity entity, const glm::vec3& delta) = 0;
  virtual void applyRotationDelta(entt::entity entity, const glm::vec3& delta) = 0;
  virtual void applyScaleDelta(entt::entity entity, const glm::vec3& delta) = 0;
};

}  // namespace engine
