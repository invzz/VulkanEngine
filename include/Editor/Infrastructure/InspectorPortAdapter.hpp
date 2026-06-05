#pragma once

#include "Engine/Application/Ports/IInspectorPort.hpp"

namespace engine {

class EngineState;
class Scene;
class JoltPhysicsSystem;

// Adapter that bridges EngineState to the inspector port.
class InspectorPortAdapter final : public IInspectorPort {
 public:
  explicit InspectorPortAdapter(EngineState& engineState);

  // Transform updates
  void updateTransform(entt::entity entity, const glm::mat4& transform) override;
  void updateTranslation(entt::entity entity, const glm::vec3& translation) override;
  void updateRotation(entt::entity entity, const glm::vec3& rotation) override;
  void updateScale(entt::entity entity, const glm::vec3& scale) override;

  // Entity state
  void setEntityActive(entt::entity entity, bool active) override;

  // Scene access
  [[nodiscard]] Scene* scene() override;
  [[nodiscard]] entt::registry& registry() override;
  [[nodiscard]] JoltPhysicsSystem* joltPhysicsSystem() override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
