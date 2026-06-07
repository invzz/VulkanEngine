#pragma once

#include <memory>

#include "Engine/Application/Ports/IRenderContextPort.hpp"

namespace engine {

    class ModelRenderSystem;
    class ShadowSystem;
    class LightSystem;
    class SkyboxRenderSystem;
    class GridRenderSystem;
    class DeferredLightingSystem;
    class PostProcessingSystem;
    class IBLSystem;
    class ColliderDebugRenderSystem;
    class SelectionOutlineSystem;
    class MorphTargetManager;
    class CameraSystem;

    struct RenderingStateView {
        ModelRenderSystem*         modelRenderSystem      = nullptr;
        ShadowSystem*              shadowSystem           = nullptr;
        LightSystem*               lightSystem            = nullptr;
        SkyboxRenderSystem*        skyboxRenderSystem     = nullptr;
        GridRenderSystem*          gridRenderSystem       = nullptr;
        DeferredLightingSystem*    deferredLightingSystem = nullptr;
        PostProcessingSystem*      postProcessingSystem   = nullptr;
        IBLSystem*                 iblSystem              = nullptr;
        CameraSystem*              camera                 = nullptr;
        ColliderDebugRenderSystem* colliderDebug          = nullptr;
        SelectionOutlineSystem*    selectionOutline       = nullptr;
        IRenderContextPort*        renderContextPort      = nullptr;
        bool*                      showSkybox             = nullptr;
        bool*                      showGrid               = nullptr;
        bool*                      showDebugObjects       = nullptr;
        bool*                      showColliderWireframes = nullptr;
        bool*                      debugMode              = nullptr;
        MorphTargetManager*        morphTargetManager     = nullptr;

        /**
   * @brief Check if all required pointers are non-null.
   * Nullable fields (showSkybox, showGrid, etc.) are excluded — they may
   * legitimately be null when the corresponding runtime toggle isn't bound.
   */
        [[nodiscard]] bool isValid() const {
            return modelRenderSystem != nullptr && shadowSystem != nullptr && lightSystem != nullptr && skyboxRenderSystem != nullptr && gridRenderSystem != nullptr && deferredLightingSystem != nullptr && postProcessingSystem != nullptr && iblSystem != nullptr && camera != nullptr && colliderDebug != nullptr && renderContextPort != nullptr && morphTargetManager != nullptr;
        }
    };

}  // namespace engine
