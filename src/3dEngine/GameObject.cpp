#include "3dEngine/GameObject.hpp"

#include "3dEngine/AnimationController.hpp"

namespace engine {
  glm::mat4 TransformComponent::modelTransform() const
  {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    // clang-format off
    return glm::mat4{
        scale.x * (c1 * c3 + s1 * s2 * s3), scale.x * (c2 * s3), scale.x * (c1 * s2 * s3 - c3 * s1), 0.0f,
        scale.y * (c3 * s1 * s2 - c1 * s3), scale.y * (c2 * c3), scale.y * (c1 * c3 * s2 + s1 * s3), 0.0f,
        scale.z * (c2 * s1),                scale.z * (-s2),     scale.z * (c1 * c2),                0.0f,
        translation.x,                      translation.y,       translation.z,                      1.0f};
    // clang-format on
  };

  glm::vec3 TransformComponent::getForwardDir() const
  {
    return glm::vec3{
            glm::sin(rotation.y) * glm::cos(rotation.x),
            -glm::sin(rotation.x),
            glm::cos(rotation.y) * glm::cos(rotation.x),
    };
  }

  glm::vec3 TransformComponent::getRightDir() const
  {
    return glm::vec3{
            glm::cos(rotation.y),
            0.0f,
            -glm::sin(rotation.y),
    };
  }

  glm::mat3 TransformComponent::normalMatrix() const
  {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);

    // clang-format off
    return glm::mat3 {
        scale.x * (c1 * c3 + s1 * s2 * s3), scale.x * (c2 * s3), scale.x * (c1 * s2 * s3 - c3 * s1), 
        scale.y * (c3 * s1 * s2 - c1 * s3), scale.y * (c2 * c3), scale.y * (c1 * c3 * s2 + s1 * s3), 
        scale.z * (c2 * s1),                scale.z * (-s2),     scale.z * (c1 * c2)               ,};
    // clang-format on
  }
} // namespace engine
