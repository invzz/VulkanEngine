#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::editor::viewport_gizmo {

    inline glm::vec3 toGizmoSpace(const glm::vec3& v) {
        return glm::vec3(v.x, -v.y, -v.z);
    }

    inline glm::vec3 fromGizmoSpace(const glm::vec3& v) {
        return glm::vec3(v.x, -v.y, -v.z);
    }

    inline glm::quat toGizmoSpace(const glm::quat& q) {
        const glm::mat3 s(1.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, 0.0f, -1.0f);
        const glm::mat3 r = glm::mat3_cast(q);
        return glm::quat_cast(s * r * s);
    }

    inline glm::quat fromGizmoSpace(const glm::quat& q) {
        const glm::mat3 s(1.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, 0.0f, -1.0f);
        const glm::mat3 r = glm::mat3_cast(q);
        return glm::quat_cast(s * r * s);
    }

}  // namespace engine::editor::viewport_gizmo
