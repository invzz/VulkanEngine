#pragma once

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/GameObject.hpp"

namespace engine {

  class CameraSystem
  {
  public:
    CameraSystem() = default;

    void update(FrameInfo& frameInfo, float aspectRatio) const;

    void updatePerspective(Camera& camera, const GameObject& cameraObject, float aspectRatio) const;

    void updateOrthographic(Camera& camera, const GameObject& cameraObject, float aspectRatio) const;

  private:
    float fieldOfViewDegrees_ = 45.0f;
    float nearPlane_          = 0.01f;
    float farPlane_           = 200.0f;
  };

} // namespace engine
