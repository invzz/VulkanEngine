#pragma once

#include "Engine/Application/Ports/IRenderContextPort.hpp"
#include "Engine/Application/StateViews/InputStateView.hpp"
#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Application/StateViews/SceneRuntimeStateView.hpp"

#include "entt/entity/fwd.hpp"

namespace engine {

    class ResourceManager;
    class AnimationSystem;
    class LODSystem;
    class PhysicsSystem;
    class JoltPhysicsSystem;
    class DescriptorPool;
    class DescriptorSetLayout;

    struct SkyboxSettings;
    struct ShadowSettings;

    struct ResourceStateView {
        ResourceManager*     resourceManager         = nullptr;
        IRenderContextPort*  renderContextPort       = nullptr;
        DescriptorPool*      gbufferPool             = nullptr;
        DescriptorSetLayout* gbufferSetLayout        = nullptr;
        DescriptorPool*      deferredIblPool         = nullptr;
        DescriptorSetLayout* deferredIblSetLayout    = nullptr;
        DescriptorPool*      deferredShadowPool      = nullptr;
        DescriptorSetLayout* deferredShadowSetLayout = nullptr;
        DescriptorPool*      postProcessPool         = nullptr;
        DescriptorSetLayout* postProcessSetLayout    = nullptr;

        /**
   * @brief Check if all required pointers are non-null.
   */
        [[nodiscard]] bool isValid() const {
            return resourceManager != nullptr && renderContextPort != nullptr && gbufferPool != nullptr && gbufferSetLayout != nullptr && deferredIblPool != nullptr && deferredIblSetLayout != nullptr && deferredShadowPool != nullptr && deferredShadowSetLayout != nullptr && postProcessPool != nullptr && postProcessSetLayout != nullptr;
        }
    };

    struct SystemServicesView {
        ObjectSelectionSystem*     objectSelection  = nullptr;
        InputSystem*               input            = nullptr;
        CameraSystem*              camera           = nullptr;
        ColliderDebugRenderSystem* colliderDebug    = nullptr;
        SelectionOutlineSystem*    selectionOutline = nullptr;
        AnimationSystem*           animation        = nullptr;
        LODSystem*                 lod              = nullptr;
        ModelRenderSystem*         modelRender      = nullptr;
        ShadowSystem*              shadow           = nullptr;
        LightSystem*               light            = nullptr;
        SkyboxRenderSystem*        skyboxRender     = nullptr;
        GridRenderSystem*          gridRender       = nullptr;
        DeferredLightingSystem*    deferredLighting = nullptr;
        PostProcessingSystem*      postProcessing   = nullptr;
        IBLSystem*                 ibl              = nullptr;
        PhysicsSystem*             physics          = nullptr;
        JoltPhysicsSystem*         joltPhysics      = nullptr;

        /**
   * @brief Check if all required pointers are non-null.
   */
        [[nodiscard]] bool isValid() const {
            return objectSelection != nullptr && input != nullptr && camera != nullptr && colliderDebug != nullptr && selectionOutline != nullptr && animation != nullptr && lod != nullptr && modelRender != nullptr && shadow != nullptr && light != nullptr && skyboxRender != nullptr && gridRender != nullptr && deferredLighting != nullptr && postProcessing != nullptr && ibl != nullptr && physics != nullptr && joltPhysics != nullptr;
        }
    };

}  // namespace engine