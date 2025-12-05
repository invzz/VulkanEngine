#pragma once

#include <vulkan/vulkan.h>

#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

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
    glm::vec4        pointLightShadowData[4];           // xyz = position, w = far plane (for cube shadows)
    int              pointLightCount       = 0;
    int              directionalLightCount = 0;
    int              spotLightCount        = 0;
    int              shadowLightCount      = 0; // Number of 2D shadow maps (directional + spot)
    int              cubeShadowLightCount  = 0; // Number of cube shadow maps (point lights)
    int              _pad1;
    int              _pad2;
    int              _pad3;
    glm::vec4        frustumPlanes[6]; // Frustum planes for culling (Left, Right, Bottom, Top, Near, Far)
  };

  struct FrameInfo
  {
    int                 frameIndex;
    float               frameTime;
    VkCommandBuffer     commandBuffer;
    Camera&             camera;
    VkDescriptorSet     globalDescriptorSet;
    VkDescriptorSet     globalTextureSet; // Bindless texture set
    Scene*              scene;            // Scene access
    uint32_t            selectedObjectId; // ID of currently selected object (0 = camera)
    entt::entity        selectedEntity;   // Selected entity handle
    entt::entity        cameraEntity;     // Camera entity handle
    MorphTargetManager* morphManager;     // Manager for morph target animations (nullptr if not used)
    VkExtent2D          extent;           // Screen extent
  };

} // namespace engine