#ifndef ENGINE_ENGINE_FACADE_HPP
#define ENGINE_ENGINE_FACADE_HPP

#include <vulkan/vulkan_core.h>

#include <memory>

#include "entt/entity/fwd.hpp"

namespace engine {

    class EngineState;
    class Renderer;
    class Device;
    class Camera;
    class Window;
    class ModelRenderSystem;
    class ShadowSystem;
    class LightSystem;
    class SkyboxRenderSystem;
    class GridRenderSystem;
    class DeferredLightingSystem;
    class PostProcessingSystem;
    class IBLSystem;
    class CameraSystem;
    class ColliderDebugRenderSystem;
    class MorphTargetManager;
    class Scene;
    class AnimationSystem;
    class PhysicsSystem;
    class JoltPhysicsSystem;
    class InputSystem;
    class ObjectSelectionSystem;
    class Keyboard;
    class Mouse;
    class DescriptorPool;
    class DescriptorSetLayout;
    class SkyboxSettings;
    class ShadowSettings;
    struct PostProcessPushConstants;

    /**
 * @brief Thin facade providing convenient access to EngineState internals.
 *
 * Replaces the pattern of passing RenderingStateView + SceneRuntimeStateView +
 * InputStateView + adapter references to render passes. Passes that need
 * specific services can still receive them directly; this facade is for
 * simple accessors that would otherwise require a state view.
 */
    class EngineFacade {
       public:
        explicit EngineFacade(EngineState& state);

        // --- Systems ---
        ModelRenderSystem&         modelRender() const;
        ShadowSystem&              shadow() const;
        LightSystem&               light() const;
        SkyboxRenderSystem&        skyboxRender() const;
        GridRenderSystem&          gridRender() const;
        DeferredLightingSystem&    deferredLighting() const;
        PostProcessingSystem&      postProcessing() const;
        IBLSystem&                 ibl() const;
        CameraSystem&              camera() const;
        ColliderDebugRenderSystem& colliderDebug() const;
        MorphTargetManager&        morphTarget() const;
        AnimationSystem&           animation() const;
        PhysicsSystem&             physics() const;
        JoltPhysicsSystem*         joltPhysics() const;
        InputSystem*               input() const;
        ObjectSelectionSystem*     objectSelection() const;

        // --- Scene ---
        Scene&       scene() const;
        entt::entity selectedEntity() const;
        entt::entity cameraEntity() const;
        void         setSelectedEntity(entt::entity e);
        void         setCameraEntity(entt::entity e);

        // --- Settings (mutable refs) ---
        bool&                     showSkybox() const;
        bool&                     showGrid() const;
        bool&                     showDebugObjects() const;
        bool&                     showColliderWireframes() const;
        bool&                     debugMode() const;
        bool&                     physicsSimulationRunning() const;
        bool&                     solidGroundEnabled() const;
        SkyboxSettings&           skySettings() const;
        ShadowSettings&           shadowSettings() const;
        PostProcessPushConstants& postProcessPush() const;

        // --- Vulkan resources ---
        VkDescriptorSet      gbufferDescriptorSet(int frameIndex) const;
        VkDescriptorSet      deferredShadowDescriptorSet(int frameIndex) const;
        VkDescriptorSet      deferredIblDescriptorSet(int frameIndex) const;
        VkDescriptorSet      postProcessDescriptorSet(int frameIndex) const;
        DescriptorSetLayout& gbufferSetLayout() const;
        DescriptorPool&      gbufferPool() const;
        DescriptorSetLayout& postProcessSetLayout() const;
        DescriptorPool&      postProcessPool() const;
        DescriptorSetLayout& deferredIblSetLayout() const;
        DescriptorPool&      deferredIblPool() const;
        DescriptorSetLayout& deferredShadowSetLayout() const;
        DescriptorPool&      deferredShadowPool() const;

        // --- Non-owned deps ---
        class IRenderContextPort& renderContext() const;
        class ResourceManager&    resourceManager() const;

        // --- Post-processing recreation ---
        void recreatePostProcessingSystem(VkDescriptorSetLayout existingLayout);

        // --- Raw access (for passes that need it) ---
        EngineState& raw() const;

       private:
        EngineState& state_;
    };

}  // namespace engine

#endif  // ENGINE_ENGINE_FACADE_HPP
