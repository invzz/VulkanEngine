#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <entt/entt.hpp>

namespace engine {

    class Scene;
    class JoltPhysicsSystem;

    class IInspectorPort {
       public:
        virtual ~IInspectorPort() = default;

        virtual void updateTransform(entt::entity entity, const glm::mat4& transform)     = 0;
        virtual void updateTranslation(entt::entity entity, const glm::vec3& translation) = 0;
        virtual void updateRotation(entt::entity entity, const glm::vec3& rotation)       = 0;
        virtual void updateScale(entt::entity entity, const glm::vec3& scale)             = 0;

        virtual void setEntityActive(entt::entity entity, bool active) = 0;

        // Scene access for UI panels
        [[nodiscard]] virtual Scene*             scene()             = 0;
        [[nodiscard]] virtual entt::registry&    registry()          = 0;
        [[nodiscard]] virtual JoltPhysicsSystem* joltPhysicsSystem() = 0;
    };

}  // namespace engine
