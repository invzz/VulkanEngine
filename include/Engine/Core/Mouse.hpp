#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_MOUSE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_MOUSE_HPP
#include <GLFW/glfw3.h>
#include <utility>

#include "Engine/Core/Window.hpp"
namespace engine {
    class Mouse {
       public:
        explicit Mouse(Window& window) : window{window} {}
        ~Mouse() = default;
        [[nodiscard]] std::pair<double, double> getCursorPosition() const;
        /**
         * @brief Update transform rotation from mouse deltas.
         *
         * Precondition: the caller has put GLFW in DISABLED cursor mode so the
         * cursor is confined and hidden. This method simply reads delta pixels
         * and applies rotation; it does not manage cursor visibility.
         */
        void lookAround(float deltaTime, struct TransformComponent& transform);
        /** Reset internal delta tracking (call when leaving navigation mode). */
        void reset();

       private:
        [[nodiscard]] GLFWwindow* getGLFWwindow() const {
            return window.getGLFWwindow();
        }
        Window& window;
        float   lookSpeed         = 1.5f;
        float   pixelSensitivity  = 45.0f / 180.0f;
        double  lastX             = 0.0;
        double  lastY             = 0.0;
        bool    mouseInitialized_ = false;
    };
}  // namespace engine
#endif
