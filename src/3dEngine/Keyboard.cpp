#include "3dEngine/Keyboard.hpp"

#include "3dEngine/AnimationController.hpp"

namespace engine {

  // namespace engine
  void engine::Keyboard::moveInPlaneXZ(float deltaTime, GameObject& gameObject) const
  {
    glm::vec3 rotation{0.0f};
    if (isKeyPressed(mappings.lookRight))
    {
      rotation.y += 1.f;
    }
    if (isKeyPressed(mappings.lookLeft))
    {
      rotation.y -= 1.f;
    }
    if (isKeyPressed(mappings.lookUp))
    {
      rotation.x += 1.f;
    }
    if (isKeyPressed(mappings.lookDown))
    {
      rotation.x -= 1.f;
    }

    if (glm::length(rotation) > std::numeric_limits<float>::epsilon())
    {
      gameObject.transform.rotation += lookSpeed * deltaTime * glm::normalize(rotation);
    }

    // clamp the pitch rotation to avoid flipping
    gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
    gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

    auto            forwardDir = gameObject.transform.getForwardDir();
    auto            rightDir   = gameObject.transform.getRightDir();
    const glm::vec3 upDir{0.0f, 1.0f, 0.0f};

    glm::vec3 movement{0.0f};
    if (isKeyPressed(mappings.moveForward))
    {
      movement += forwardDir;
    }
    if (isKeyPressed(mappings.moveBackward))
    {
      movement -= forwardDir;
    }
    if (isKeyPressed(mappings.moveRight))
    {
      movement += rightDir;
    }
    if (isKeyPressed(mappings.moveLeft))
    {
      movement -= rightDir;
    }
    if (isKeyPressed(mappings.moveUp))
    {
      movement -= upDir;
    }
    if (isKeyPressed(mappings.moveDown))
    {
      movement += upDir;
    }

    if (glm::length(movement) > std::numeric_limits<float>::epsilon())
    {
      gameObject.transform.translation += moveSpeed * deltaTime * glm::normalize(movement);
    }
  }

} // namespace engine