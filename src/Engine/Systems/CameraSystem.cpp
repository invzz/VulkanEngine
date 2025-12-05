#include "Engine/Systems/CameraSystem.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  void CameraSystem::update(FrameInfo& frameInfo, float aspectRatio) const
  {
    auto& registry = frameInfo.scene->getRegistry();
    if (registry.valid(frameInfo.cameraEntity))
    {
      // Check if the entity has the required components
      if (registry.all_of<CameraComponent, TransformComponent>(frameInfo.cameraEntity))
      {
        auto&       cameraComp = registry.get<CameraComponent>(frameInfo.cameraEntity);
        const auto& transform  = registry.get<TransformComponent>(frameInfo.cameraEntity);

        updateCamera(cameraComp, transform, aspectRatio);

        // Sync the frameInfo camera with the component camera
        // This ensures the renderer uses the updated camera matrices
        frameInfo.camera = cameraComp.camera;
      }
    }
  }

  void CameraSystem::updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio) const
  {
    // Update projection
    if (!cameraComp.isOrthographic)
    {
      cameraComp.camera.setPerspectiveProjection(glm::radians(cameraComp.fovY), aspectRatio, cameraComp.nearZ, cameraComp.farZ);
    }
    else
    {
      // For orthographic, we need to define the bounds.
      // Assuming orthoSize is the height, width is derived from aspect ratio.
      float orthoHeight = cameraComp.orthoSize;
      float orthoWidth  = aspectRatio * orthoHeight;
      cameraComp.camera.setOrtographicProjection(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, cameraComp.nearZ, cameraComp.farZ);
    }

    // Update view
    cameraComp.camera.setViewYXZ(transform.translation, transform.rotation);

    // Update frustum
    cameraComp.camera.updateFrustum();
  }
} // namespace engine
