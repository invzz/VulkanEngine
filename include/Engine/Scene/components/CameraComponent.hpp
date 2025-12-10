#pragma once

#include "Engine/Scene/Camera.hpp"

namespace engine {

  struct CameraComponent
  {
    Camera camera{};

    // Perspective settings
    float fovY  = 80.0f;
    float nearZ = 0.1f;
    float farZ  = 100.0f;

    // Orthographic settings
    float orthoSize = 10.0f; // Vertical size

    bool isOrthographic = false;
    bool isPrimary      = true; // To identify the main camera if multiple exist
  };

} // namespace engine
