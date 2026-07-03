#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_FRAMEINFO_HPP

#include <vulkan/vulkan.h>

#include <cstddef>

#include "Engine/EditorState.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

    class MorphTargetManager;

    constexpr size_t maxShadowLightCount = 8;

    struct PointLight {
        glm::vec4 positionRadius2;
        glm::vec4 colorIntensity;
    };

    struct DirectionalLight {
        glm::vec4 direction;
        glm::vec4 color;
    };

    struct SpotLight {
        glm::vec4 positionRadius2;
        glm::vec4 directionInner;
        glm::vec4 colorIntensity;
        glm::vec4 attenOuter;
    };

    struct GlobalUbo {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 invProjection{1.0f};
        glm::mat4 invView{1.0f};
        glm::vec4 lightAmbient{1.f, 1.0f, 1.0f, .02f};
        glm::vec4 cameraPosition;
        glm::mat4 lightSpaceMatrices[maxShadowLightCount];

        glm::vec4 pointLightShadowData[4];

        int pointLightCount       = 0;
        int directionalLightCount = 0;
        int spotLightCount        = 0;
        int shadowLightCount      = 0;
        int cubeShadowLightCount  = 0;
        int debugMode             = 0;
        int _padDebug             = 0;

        int       _padDebug0 = 0;
        glm::vec4 frustumPlanes[6];
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
        VkDescriptorSet          globalTextureSet;
        Scene*                   scene;
        uint32_t                 selectedObjectId;
        entt::entity             selectedEntity;
        entt::entity             cameraEntity;
        MorphTargetManager*      morphManager;
        VkExtent2D               extent;
        ViewportMode             viewportMode;
        glm::vec2                viewportMousePos{};
        bool                     viewportMouseClicked{};
        int                      debugMode{0};
        class ModelRenderSystem* modelRenderSystem{nullptr};
        class ShadowSystem*      shadowSystem{nullptr};

        int  gizmoOperation{0};
        int  gizmoMode{1};
        bool gizmoEnabled{true};
        bool viewGizmoOrbitSelected{true};
    };

}  // namespace engine

#endif
