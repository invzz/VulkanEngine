#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP

#include <vulkan/vulkan.h>

#include <cstddef>

#include "Engine/EditorState.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

    class MorphTargetManager;

    // Maximum number of 2D shadow-casting lights stored in the UBO.
    // (Dynamic lighting itself is unbounded via SSBOs.)
    constexpr size_t maxShadowLightCount = 8;

    struct PointLight {
        glm::vec4 positionRadius2;  // xyz = position, w = radius^2
        glm::vec4 colorIntensity;   // rgb = color, w = intensity
    };

    struct DirectionalLight {
        glm::vec4 direction;  // w component unused (direction should be normalized)
        glm::vec4 color;      // w component is intensity
    };

    struct SpotLight {
        glm::vec4 positionRadius2;  // xyz = position, w = radius^2
        glm::vec4 directionInner;   // xyz = direction, w = inner cutoff angle (cos)
        glm::vec4 colorIntensity;   // rgb = color, w = intensity
        glm::vec4 attenOuter;       // x = outer cutoff (cos), y = constant, z = linear, w = quadratic
    };

    struct GlobalUbo {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 invProjection{1.0f};  // CPU-provided inverse projection (stored in UBO)
        glm::mat4 invView{1.0f};        // CPU-provided inverse view (stored in UBO)
        glm::vec4 lightAmbient{1.f, 1.0f, 1.0f, .02f};
        glm::vec4 cameraPosition;
        glm::mat4 lightSpaceMatrices[maxShadowLightCount];  // Light space transformation
                                                            // matrices for shadows
        glm::vec4 pointLightShadowData[4];                  // xyz = position, w = far plane (for cube
                                                            // shadows)
        int pointLightCount       = 0;
        int directionalLightCount = 0;
        int spotLightCount        = 0;
        int shadowLightCount      = 0;  // Number of 2D shadow maps (dir cascades + spots)
        int cubeShadowLightCount  = 0;  // Number of cube shadow maps (point lights)
        int debugMode             = 0;  // 0: None, 1: Albedo, 2: Normal, 3: Roughness, 4: Metallic, 5: Lighting, 6: Emissive Only
        int _padDebug             = 0;
        // std140 requires vec4-aligned data after arrays of ints; add padding to match std140 layout.
        int       _padDebug0 = 0;
        glm::vec4 frustumPlanes[6];  // Frustum planes for culling (Left, Right,
                                     // Bottom, Top, Near, Far)
    };

    struct GlobalUboCold {
        glm::vec4 reservedCold{0.0f};
    };

    static_assert(offsetof(GlobalUbo, frustumPlanes) % 16 == 0, "GlobalUbo::frustumPlanes must be 16-byte aligned for std140");
    static_assert(sizeof(GlobalUbo) % 16 == 0, "GlobalUbo size must be a multiple of 16 bytes for std140");
    static_assert(sizeof(GlobalUboCold) % 16 == 0, "GlobalUboCold size must be a multiple of 16 bytes for std140");

    struct FrameInfo {
        int                      frameIndex;
        float                    frameTime;
        VkCommandBuffer          commandBuffer;
        Camera&                  camera;
        VkDescriptorSet          globalDescriptorSet;
        VkDescriptorSet          globalTextureSet;            // Bindless texture set
        Scene*                   scene;                       // Scene access
        uint32_t                 selectedObjectId;            // ID of currently selected object (0 = camera)
        entt::entity             selectedEntity;              // Selected entity handle
        entt::entity             cameraEntity;                // Camera entity handle
        MorphTargetManager*      morphManager;                // Manager for morph target animations (nullptr if not used)
        VkExtent2D               extent;                      // Screen extent
        ViewportMode             viewportMode;                // Current viewport interaction mode
        glm::vec2                viewportMousePos{};          // Mouse position in viewport-normalized [0,1] (when available)
        bool                     viewportMouseClicked{};      // True if a picking click started this frame in the viewport
        int                      debugMode{0};                // Mirrors GlobalUbo::debugMode for pipeline selection
        class ModelRenderSystem* modelRenderSystem{nullptr};  // Model rendering system
        class ShadowSystem*      shadowSystem{nullptr};       // Shadow rendering system

        // Gizmo state
        int  gizmoOperation{0};  // ImGuizmo::OPERATION (TRANSLATE/ROTATE/SCALE)
        int  gizmoMode{1};       // ImGuizmo::MODE (LOCAL=0, WORLD=1)
        bool gizmoEnabled{true};
        bool viewGizmoOrbitSelected{true};
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP
