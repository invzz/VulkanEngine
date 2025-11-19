#pragma once

#include <vulkan/vulkan.h>

#include "AnimationController.hpp"
#include "Camera.hpp"
#include "GameObject.hpp"

namespace engine {

  class MorphTargetManager;

  constexpr size_t maxLightCount = 16;

  struct PointLight
  {
    glm::vec4 position; // w component unused
    glm::vec4 color;    // w component is intensity
  };

  struct DirectionalLight
  {
    glm::vec4 direction; // w component unused (direction should be normalized)
    glm::vec4 color;     // w component is intensity
  };

  struct SpotLight
  {
    glm::vec4 position;       // w component unused
    glm::vec4 direction;      // w component is inner cutoff angle (cos)
    glm::vec4 color;          // w component is intensity
    float     outerCutoff;    // Outer cutoff angle (cos)
    float     constantAtten;  // Constant attenuation
    float     linearAtten;    // Linear attenuation
    float     quadraticAtten; // Quadratic attenuation
  };

  struct GlobalUbo
  {
    glm::mat4        projection{1.0f};
    glm::mat4        view{1.0f};
    glm::vec4        lightAmbient{1.f, 1.0f, 1.0f, .02f};
    glm::vec4        cameraPosition;
    PointLight       pointLights[maxLightCount];
    DirectionalLight directionalLights[maxLightCount];
    SpotLight        spotLights[maxLightCount];
    glm::mat4        lightSpaceMatrices[maxLightCount]; // Light space transformation matrices for shadows
    int              pointLightCount       = 0;
    int              directionalLightCount = 0;
    int              spotLightCount        = 0;
    int              shadowLightCount      = 0; // Number of lights casting shadows
  };

  struct FrameInfo
  {
    int                 frameIndex;
    float               frameTime;
    VkCommandBuffer     commandBuffer;
    Camera&             camera;
    VkDescriptorSet     globalDescriptorSet;
    GameObject::Map&    gameObjects;
    GameObject::id_t    selectedObjectId; // ID of currently selected object (0 = camera)
    GameObject*         selectedObject;   // Pointer to selected object (nullptr = camera)
    GameObject&         cameraObject;
    MorphTargetManager* morphManager; // Manager for morph target animations (nullptr if not used)
  };

} // namespace engine