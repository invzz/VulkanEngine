#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "Model.hpp"

namespace engine {

    struct TransformComponent
    {
        glm::vec3 translation{};           // position offset
        glm::vec3 scale{1.0f, 1.0f, 1.0f}; // scaling factors
        glm::vec3 rotation{};              // rotation angles in radians

        // Matrix corresponding to translate * rotate * scale
        // optimized version using precomputed sines and cosines
        // Note: rotation order is Y (yaw), X (pitch), Z (roll)
        // Reference: https://en.wikipedia.org/wiki/Rotation_matrix#In_three_dimensions
        glm::mat4 mat4() const
        {
            const float c3 = glm::cos(rotation.z);
            const float s3 = glm::sin(rotation.z);
            const float c2 = glm::cos(rotation.x);
            const float s2 = glm::sin(rotation.x);
            const float c1 = glm::cos(rotation.y);
            const float s1 = glm::sin(rotation.y);
            // clang-format off
            return glm::mat4{
                scale.x * (c1 * c3 + s1 * s2 * s3),   scale.x * (c2 * s3),          scale.x * (c1 * s2 * s3 - c3 * s1), 0.0f,
                scale.y * (c3 * s1 * s2 - c1 * s3),   scale.y * (c2 * c3),          scale.y * (c1 * c3 * s2 + s1 * s3), 0.0f,
                scale.z * (c2 * s1),                  scale.z * (-s2),              scale.z * (c1 * c2),                0.0f,
                translation.x,                        translation.y,                translation.z,                      1.0f  
            };
        };
        // clang-format on
    };

    using id_t = unsigned int;

    class GameObject
    {
      public:
        std::shared_ptr<Model> model{};
        glm::vec3              color{};
        TransformComponent     transform{};
        id_t                   id{0};

        static GameObject createGameObjectWithId()
        {
            static id_t currentId = 0;
            return GameObject{currentId++};
        }

        // delete copy operations
        GameObject(const GameObject&)            = delete;
        GameObject& operator=(const GameObject&) = delete;

        // default move operations
        GameObject(GameObject&&) noexcept            = default;
        GameObject& operator=(GameObject&&) noexcept = default;

        id_t getId() const { return id; }

      private:
        explicit GameObject(id_t objId) : id{objId} {}
    };

} // namespace engine
