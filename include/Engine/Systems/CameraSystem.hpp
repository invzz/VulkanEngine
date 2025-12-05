#pragma once

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  class CameraSystem
  {
  public:
    CameraSystem() = default;

    void update(FrameInfo& frameInfo, float aspectRatio) const;

  private:
    void updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio) const;
  };

} // namespace engine
