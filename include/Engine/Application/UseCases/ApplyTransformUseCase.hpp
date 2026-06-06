#pragma once

#include <glm/glm.hpp>

#include "Engine/Application/Ports/ITransformPort.hpp"

namespace engine {

    // Use case for applying transform changes to a selected entity.
    class ApplyTransformUseCase {
       public:
        explicit ApplyTransformUseCase(ITransformPort& transformPort);

        // Apply translation changes.
        void executeTranslation(entt::entity entity, const glm::vec3& translation);

        // Apply rotation changes.
        void executeRotation(entt::entity entity, const glm::vec3& rotation);

        // Apply scale changes.
        void executeScale(entt::entity entity, const glm::vec3& scale);

        // Apply delta translation.
        void executeTranslationDelta(entt::entity entity, const glm::vec3& delta);

        // Apply delta rotation.
        void executeRotationDelta(entt::entity entity, const glm::vec3& delta);

        // Apply delta scale.
        void executeScaleDelta(entt::entity entity, const glm::vec3& delta);

       private:
        ITransformPort& transformPort_;
    };

}  // namespace engine
