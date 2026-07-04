#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_CAMERACOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_CAMERACOMPONENT_HPP
#include "Engine/Scene/Camera.hpp"
namespace engine {
    struct CameraComponent {
        Camera camera;
        float  fovY           = 80.0f;
        float  nearZ          = 0.1f;
        float  farZ           = 100.0f;
        float  orthoSize      = 10.0f;
        bool   isOrthographic = false;
        bool   isPrimary      = true;
    };
}  // namespace engine
#endif
