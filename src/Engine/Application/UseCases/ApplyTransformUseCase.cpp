#include "Engine/Application/UseCases/ApplyTransformUseCase.hpp"

namespace engine {

    ApplyTransformUseCase::ApplyTransformUseCase(ITransformPort& transformPort)
        : transformPort_(transformPort) {}

    void ApplyTransformUseCase::executeTranslation(entt::entity entity, const glm::vec3& translation) {
        transformPort_.setTranslation(entity, translation);
    }

    void ApplyTransformUseCase::executeRotation(entt::entity entity, const glm::vec3& rotation) {
        transformPort_.setRotation(entity, rotation);
    }

    void ApplyTransformUseCase::executeScale(entt::entity entity, const glm::vec3& scale) {
        transformPort_.setScale(entity, scale);
    }

    void ApplyTransformUseCase::executeTranslationDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyTranslationDelta(entity, delta);
    }

    void ApplyTransformUseCase::executeRotationDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyRotationDelta(entity, delta);
    }

    void ApplyTransformUseCase::executeScaleDelta(entt::entity entity, const glm::vec3& delta) {
        transformPort_.applyScaleDelta(entity, delta);
    }

}  // namespace engine
