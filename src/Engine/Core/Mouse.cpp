#include "Engine/Core/Mouse.hpp"

#include <tuple>
#include <utility>

#include "Engine/Scene/components/TransformComponent.hpp"

#include "GLFW/glfw3.h"
#include "glm/common.hpp"
#include "glm/gtc/constants.hpp"

namespace engine {

    std::pair<double, double> Mouse::getCursorPosition() const {
        double xPos;
        double yPos;
        glfwGetCursorPos(window.getGLFWwindow(), &xPos, &yPos);
        return {xPos, yPos};
    }

    void Mouse::lookAround(float deltaTime, TransformComponent& transform) {
        double xpos;
        double ypos;
        std::tie(xpos, ypos) = getCursorPosition();

        if (!mouseInitialized_) {
            lastX             = xpos;
            lastY             = ypos;
            mouseInitialized_ = true;
            return;
        }

        auto xoffset = static_cast<float>(xpos - lastX) * pixelSensitivity;
        auto yoffset = static_cast<float>(lastY - ypos) * pixelSensitivity;

        lastX = xpos;
        lastY = ypos;

        transform.rotation.y += xoffset * lookSpeed * deltaTime;
        transform.rotation.x += yoffset * lookSpeed * deltaTime;

        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    }

    void Mouse::reset() {
        mouseInitialized_ = false;
        lastX             = 0.0;
        lastY             = 0.0;
    }

}  // namespace engine
