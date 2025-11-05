#pragma once
// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace engine {

  class Camera
  {
  public:
    Camera() = default;
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }
    const glm::mat4& getViewMatrix() const { return viewMatrix; }
    void             setPerspectiveProjection(float fovY, float aspect, float nearZ, float farZ);
    void             setOrtographicProjection(float left, float right, float bottom, float top, float nearZ, float farZ);
    void             setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
    void             setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
    void             setViewYXZ(glm::vec3 position, glm::vec3 rotation);

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }
    const glm::mat4& getInverseView() const { return inverseViewMatrix; }
    glm::vec3        getPosition() const { return glm::vec3(inverseViewMatrix[3]); }

  private:
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 inverseViewMatrix{1.0f};
  };

} // namespace engine