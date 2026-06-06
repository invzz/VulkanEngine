#include "Engine/Application/UseCases/TransformManipulationUseCase.hpp"

namespace engine {

    TransformManipulationUseCase::TransformManipulationUseCase(ITransformPort& transformPort)
        : transformPort_(transformPort) {}

    glm::vec3 TransformManipulationUseCase::getTranslation(entt::entity entity) {
        return transformPort_.getTranslation(entity);
    }

    glm::vec3 TransformManipulationUseCase::getRotation(entt::entity entity) {
        return transformPort_.getRotation(entity);
    }

    glm::vec3 TransformManipulationUseCase::getScale(entt::entity entity) {
        return transformPort_.getScale(entity);
    }

    void TransformManipulationUseCase::setTranslation(entt::entity entity, const glm::vec3& translation) {
        transformPort_.setTranslation(entity, translation);
    }

    void TransformManipulationUseCase::setRotation(entt::entity entity, const glm::vec3& rotation) {
        transformPort_.setRotation(entity, rotation);
    }

    void TransformManipulationUseCase::setScale(entt::entity entity, const glm::vec3& scale) {
        transformPort_.setScale(entity, scale);
    }

    void TransformManipulationUseCase::applyTranslationDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyTranslationDelta(entity, delta);
    }

    void TransformManipulationUseCase::applyRotationDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyRotationDelta(entity, delta);
    }

    void TransformManipulationUseCase::applyScaleDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyScaleDelta(entity, delta);
    }

}  // namespace engine
