#include "Editor/Infrastructure/TransformAdapter.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

    TransformAdapter::TransformAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    glm::vec3 TransformAdapter::getTranslation(entt::entity entity) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            return registry.get<TransformComponent>(entity).translation;
        }
        return glm::vec3{};
    }

    glm::vec3 TransformAdapter::getRotation(entt::entity entity) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            return registry.get<TransformComponent>(entity).rotation;
        }
        return glm::vec3{};
    }

    glm::vec3 TransformAdapter::getScale(entt::entity entity) {
        auto& registry = engineState_.sceneRef().getRegistry();
        if (registry.all_of<TransformComponent>(entity)) {
            return registry.get<TransformComponent>(entity).scale;
        }
        return glm::vec3{1.0f, 1.0f, 1.0f};
    }

    void TransformAdapter::setTranslation(entt::entity entity, const glm::vec3& translation) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.translation = translation;
    }

    void TransformAdapter::setRotation(entt::entity entity, const glm::vec3& rotation) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.rotation    = rotation;
    }

    void TransformAdapter::setScale(entt::entity entity, const glm::vec3& scale) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.scale       = scale;
    }

    void TransformAdapter::applyTranslationDelta(entt::entity entity, const glm::vec3& delta) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.translation += delta;
    }

    void TransformAdapter::applyRotationDelta(entt::entity entity, const glm::vec3& delta) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.rotation += delta;
    }

    void TransformAdapter::applyScaleDelta(entt::entity entity, const glm::vec3& delta) {
        auto& registry = engineState_.sceneRef().getRegistry();
        auto& tc       = registry.get<TransformComponent>(entity);
        tc.scale += delta;
    }

}  // namespace engine
