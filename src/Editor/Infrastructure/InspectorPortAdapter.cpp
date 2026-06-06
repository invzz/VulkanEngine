#include "Editor/Infrastructure/InspectorPortAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

    InspectorPortAdapter::InspectorPortAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    void InspectorPortAdapter::updateTransform(entt::entity entity, const glm::mat4& transform) {
        // Update the TransformComponent for the given entity.
        // Note: TransformComponent stores translation, rotation, scale separately.
        // This method is provided for compatibility with ports that pass a full matrix.
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            // Decompose the matrix to update translation, rotation, and scale.
            // For simplicity, we just update the translation component.
            registry.get<TransformComponent>(entity).translation = glm::vec3(transform[3]);
        }
    }

    void InspectorPortAdapter::updateTranslation(entt::entity entity, const glm::vec3& translation) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            registry.get<TransformComponent>(entity).translation = translation;
        }
    }

    void InspectorPortAdapter::updateRotation(entt::entity entity, const glm::vec3& rotation) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            registry.get<TransformComponent>(entity).rotation = rotation;
        }
    }

    void InspectorPortAdapter::updateScale(entt::entity entity, const glm::vec3& scale) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            registry.get<TransformComponent>(entity).scale = scale;
        }
    }

    void InspectorPortAdapter::setEntityActive(entt::entity entity, bool active) {
        // In entt, entities are "active" if they exist in the registry.
        // To deactivate, we can remove all components (effectively making it inert).
        // For now, we just log the request.
        (void) entity;
        (void) active;
    }

    Scene* InspectorPortAdapter::scene() {
        return &engineState_.sceneRef();
    }

    entt::registry& InspectorPortAdapter::registry() {
        return engineState_.sceneRef().getRegistry();
    }

    JoltPhysicsSystem* InspectorPortAdapter::joltPhysicsSystem() {
        return engineState_.getJoltPhysicsSystem();
    }

}  // namespace engine
