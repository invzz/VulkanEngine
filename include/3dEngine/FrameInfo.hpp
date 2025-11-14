#pragma once

#include <vulkan/vulkan.h>

#include "Camera.hpp"
#include "GameObject.hpp"

namespace engine {

  constexpr size_t maxLightCount = 16;

  struct PointLight
  {
    glm::vec4 position; // w component unused
    glm::vec4 color;    // w component is intensity
  };

  struct GlobalUbo
  {
    glm::mat4  projection{1.0f};
    glm::mat4  view{1.0f};
    glm::vec4  lightAmbient{1.f, 1.0f, 1.0f, .02f};
    glm::vec4  cameraPosition;
    PointLight pointLights[maxLightCount];
    int        lightCount = 0;
  };

  struct FrameInfo
  {
    int              frameIndex;
    float            frameTime;
    VkCommandBuffer  commandBuffer;
    Camera&          camera;
    VkDescriptorSet  globalDescriptorSet;
    GameObject::Map& gameObjects;
    GameObject::id_t selectedObjectId; // ID of currently selected object (0 = camera)
    GameObject*      selectedObject;   // Pointer to selected object (nullptr = camera)
  };

} // namespace engine