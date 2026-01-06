#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP

#include <vulkan/vulkan.h>

#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

  class MorphTargetManager;

  // Maximum number of 2D shadow-casting lights stored in the UBO.
  // (Dynamic lighting itself is unbounded via SSBOs.)
  constexpr size_t maxShadowLightCount = 16;

  struct PointLight
  {
    glm::vec4 position; // w component unused
    glm::vec4 color;    // w component is intensity
    float     radius2{0.0f};
    float     _pad0{0.0f};
    float     _pad1{0.0f};
    float     _pad2{0.0f};
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
    float     radius2{0.0f};
    float     _pad0{0.0f};
    float     _pad1{0.0f};
    float     _pad2{0.0f};
  };

  struct GlobalUbo
  {
    glm::mat4 projection{1.0f};
    glm::mat4 view{1.0f};
    glm::vec4 lightAmbient{1.f, 1.0f, 1.0f, .02f};
    glm::vec4 cameraPosition;
    glm::mat4 lightSpaceMatrices[maxShadowLightCount]; // Light space transformation
                                                       // matrices for shadows
    glm::vec4 pointLightShadowData[4];                 // xyz = position, w = far plane (for cube
                                                       // shadows)
    glm::vec4 directionalCascadeSplits{0.0f};          // View-space split distances (x,y,z,w)
    int       pointLightCount             = 0;
    int       directionalLightCount       = 0;
    int       spotLightCount              = 0;
    int       shadowLightCount            = 0; // Number of 2D shadow maps (dir cascades + spots)
    int       cubeShadowLightCount        = 0; // Number of cube shadow maps (point lights)
    int       directionalCascadeCount     = 0;
    int       directionalCascadeBaseIndex = 0; // Index into lightSpaceMatrices / shadowMaps
    int       debugMode                   = 0; // 0: None, 1: Albedo, 2: Normal, 3: Roughness, 4: Metallic, 5: Lighting
    int       _pad2;
    glm::vec4 frustumPlanes[6]; // Frustum planes for culling (Left, Right,
                                // Bottom, Top, Near, Far)
    glm::vec4 fogColor;         // xyz = Horizon Color, w = density
    glm::vec4 fogZenithColor;   // xyz = Zenith Color, w = unused
    float     fogHeight;
    float     fogHeightDensity;
    float     _pad4;
    float     _pad5;
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
    int                 debugMode{0};     // Mirrors GlobalUbo::debugMode for pipeline selection
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP
