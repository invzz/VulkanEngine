#pragma once

#include <glm/glm.hpp>

#include "Engine/Application/Ports/ITransformPort.hpp"

namespace engine {

// Use case for transform manipulation (set, get, delta apply).
// Aggregates ApplyTransformUseCase operations.
class TransformManipulationUseCase {
 public:
  explicit TransformManipulationUseCase(ITransformPort& transformPort);

  // Get the current transform of an entity.
  [[nodiscard]] glm::vec3 getTranslation(entt::entity entity);
  [[nodiscard]] glm::vec3 getRotation(entt::entity entity);
  [[nodiscard]] glm::vec3 getScale(entt::entity entity);

  // Set the transform of an entity.
  void setTranslation(entt::entity entity, const glm::vec3& translation);
  void setRotation(entt::entity entity, const glm::vec3& rotation);
  void setScale(entt::entity entity, const glm::vec3& scale);

  // Apply a delta transform to an entity.
  void applyTranslationDelta(entt::entity entity, const glm::vec3& delta);
  void applyRotationDelta(entt::entity entity, const glm::vec3& delta);
  void applyScaleDelta(entt::entity entity, const glm::vec3& delta);

 private:
  ITransformPort& transformPort_;
};

}  // namespace engine
