#include "3dEngine/systems/CameraSystem.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3dEngine/Camera.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"

namespace engine {

  void CameraSystem::update(FrameInfo& frameInfo, float aspectRatio) const
  {
    updatePerspective(frameInfo.camera, frameInfo.cameraObject, aspectRatio);
  }

  void CameraSystem::updatePerspective(Camera& camera, const GameObject& cameraObject, float aspectRatio) const
  {
    camera.setPerspectiveProjection(glm::radians(fieldOfViewDegrees_), aspectRatio, nearPlane_, farPlane_);
    camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);
    camera.updateFrustum(); // Update frustum planes for culling
  }

  void CameraSystem::updateOrthographic(Camera& camera, const GameObject& cameraObject, float aspectRatio) const
  {
    const float orthoHeight = 1.0f;
    const float orthoWidth  = aspectRatio * orthoHeight;
    camera.setOrtographicProjection(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, nearPlane_, farPlane_);
    camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);
  }

} // namespace engine
