#include "Engine/Core/Keyboard.hpp"

#include <glm/ext/vector_float3.hpp>

#include <limits>

#include "Engine/Scene/components/TransformComponent.hpp"

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"

namespace engine {

    void engine::Keyboard::moveInPlaneXZ(float deltaTime, TransformComponent& transform) const {
        glm::vec3 rotation{0.0f};
        if (isKeyPressed(mappings.lookRight)) {
            rotation.y += 1.f;
        }
        if (isKeyPressed(mappings.lookLeft)) {
            rotation.y -= 1.f;
        }
        if (isKeyPressed(mappings.lookUp)) {
            rotation.x += 1.f;
        }
        if (isKeyPressed(mappings.lookDown)) {
            rotation.x -= 1.f;
        }

        if (glm::length(rotation) > std::numeric_limits<float>::epsilon()) {
            transform.rotation += lookSpeed * deltaTime * glm::normalize(rotation);
        }

        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());

        auto                forwardDir = transform.getForwardDir();
        auto                rightDir   = transform.getRightDir();
        constexpr glm::vec3 upDir{0.0f, 1.0f, 0.0f};

        glm::vec3 movement{0.0f};
        if (isKeyPressed(mappings.moveForward)) {
            movement += forwardDir;
        }
        if (isKeyPressed(mappings.moveBackward)) {
            movement -= forwardDir;
        }
        if (isKeyPressed(mappings.moveRight)) {
            movement += rightDir;
        }
        if (isKeyPressed(mappings.moveLeft)) {
            movement -= rightDir;
        }
        if (isKeyPressed(mappings.moveUp)) {
            movement -= upDir;
        }
        if (isKeyPressed(mappings.moveDown)) {
            movement += upDir;
        }

        if (glm::length(movement) > std::numeric_limits<float>::epsilon()) {
            transform.translation += moveSpeed * deltaTime * glm::normalize(movement);
        }
    }

}  // namespace engine