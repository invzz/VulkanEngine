#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_TRANSFORMCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_TRANSFORMCOMPONENT_HPP
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace engine {
    struct TransformComponent {
        glm::vec3               translation{};
        glm::vec3               scale{1.0f, 1.0f, 1.0f};
        glm::vec3               rotation{};
        glm::vec3               baseScale{1.0f, 1.0f, 1.0f};
        [[nodiscard]] glm::mat4 modelTransform() const {
            const float c3 = glm::cos(rotation.z);
            const float s3 = glm::sin(rotation.z);
            const float c2 = glm::cos(rotation.x);
            const float s2 = glm::sin(rotation.x);
            const float c1 = glm::cos(rotation.y);
            const float s1 = glm::sin(rotation.y);
            return glm::mat4{
                scale.x * (c1 * c3 + s1 * s2 * s3), scale.x * (c2 * s3), scale.x * (c1 * s2 * s3 - c3 * s1), 0.0f,
                scale.y * (c3 * s1 * s2 - c1 * s3), scale.y * (c2 * c3), scale.y * (c1 * c3 * s2 + s1 * s3), 0.0f,
                scale.z * (c2 * s1), scale.z * (-s2), scale.z * (c1 * c2), 0.0f,
                translation.x, translation.y, translation.z, 1.0f};
        }
        [[nodiscard]] glm::vec3 getForwardDir() const {
            return glm::vec3{
                glm::sin(rotation.y) * glm::cos(rotation.x),
                -glm::sin(rotation.x),
                glm::cos(rotation.y) * glm::cos(rotation.x),
            };
        }
        [[nodiscard]] glm::vec3 getRightDir() const {
            return glm::vec3{
                glm::cos(rotation.y),
                0.0f,
                -glm::sin(rotation.y),
            };
        }
        [[nodiscard]] glm::mat3 normalMatrix() const {
            const float c3 = glm::cos(rotation.z);
            const float s3 = glm::sin(rotation.z);
            const float c2 = glm::cos(rotation.x);
            const float s2 = glm::sin(rotation.x);
            const float c1 = glm::cos(rotation.y);
            const float s1 = glm::sin(rotation.y);
            return glm::mat3{
                scale.x * (c1 * c3 + s1 * s2 * s3),
                scale.x * (c2 * s3),
                scale.x * (c1 * s2 * s3 - c3 * s1),
                scale.y * (c3 * s1 * s2 - c1 * s3),
                scale.y * (c2 * c3),
                scale.y * (c1 * c3 * s2 + s1 * s3),
                scale.z * (c2 * s1),
                scale.z * (-s2),
                scale.z * (c1 * c2),
            };
        }
        void lookAt(const glm::vec3& target) {
            glm::vec3 const direction = glm::normalize(target - translation);
            rotation.y                = std::atan2(direction.x, direction.z);
            rotation.x                = -std::asin(direction.y);
            rotation.z                = 0.0f;
        }
    };
}  // namespace engine
#endif
