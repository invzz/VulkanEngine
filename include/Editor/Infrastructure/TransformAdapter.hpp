#pragma once

#include <glm/glm.hpp>

#include "Engine/Application/Ports/ITransformPort.hpp"

namespace engine {

    class EngineState;

    // Adapter that bridges EngineState to the transform port.
    class TransformAdapter final : public ITransformPort {
       public:
        explicit TransformAdapter(EngineState& engineState);

        [[nodiscard]] glm::vec3 getTranslation(entt::entity entity) override;
        [[nodiscard]] glm::vec3 getRotation(entt::entity entity) override;
        [[nodiscard]] glm::vec3 getScale(entt::entity entity) override;

        void setTranslation(entt::entity entity, const glm::vec3& translation) override;
        void setRotation(entt::entity entity, const glm::vec3& rotation) override;
        void setScale(entt::entity entity, const glm::vec3& scale) override;

        void applyTranslationDelta(entt::entity entity, const glm::vec3& delta) override;
        void applyRotationDelta(entt::entity entity, const glm::vec3& delta) override;
        void applyScaleDelta(entt::entity entity, const glm::vec3& delta) override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
