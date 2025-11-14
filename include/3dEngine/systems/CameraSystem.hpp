#pragma once

#include "../Camera.hpp"
#include "../FrameInfo.hpp"
#include "../GameObject.hpp"

namespace engine {

  class CameraSystem
  {
  public:
    CameraSystem() = default;

    void update(FrameInfo& frameInfo, const GameObject& cameraObject, float aspectRatio) const;

    void updatePerspective(Camera& camera, const GameObject& cameraObject, float aspectRatio) const;

    void updateOrthographic(Camera& camera, const GameObject& cameraObject, float aspectRatio) const;

  private:
    float fieldOfViewDegrees_ = 45.0f;
    float nearPlane_          = 0.1f;
    float farPlane_           = 50.0f;
  };

} // namespace engine
