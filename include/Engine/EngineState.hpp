#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/State/StateServices.hpp"
#include "Engine/State/StateViews.hpp"
#include "Engine/SystemRegistry.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    class Device;
    class Renderer;
    class Keyboard;
    class Mouse;
    class RenderContext;
    class Window;

    // EngineState is the single source-of-truth for owned systems, scene and
    // runtime settings. Pass a pointer/reference to render passes and systems
    // so they can access the current runtime state without long parameter lists.
    class EngineState {
       public:
        ~EngineState();

        // lifecycle
        void initialize(Device& device,
            Renderer&           renderer,
            ResourceManager&    resourceManager,
            RenderContext*      requiredRenderContext,
            Window*             window,
            bool                multithreadedRecordingEnabled,
            uint32_t            multithreadedRecordingThreads);

       private:
        friend class RenderingStateService;
        friend class SceneRuntimeService;
        friend class InputStateService;
        friend class ResourceStateService;
        friend class AnimationRuntimeService;
        friend class PhysicsRuntimeService;

        [[nodiscard]] RenderingStateView renderingState() {
            return RenderingStateView{
                .modelRenderSystem      = modelRenderSystem.get(),
                .shadowSystem           = shadowSystem.get(),
                .lightSystem            = lightSystem.get(),
                .skyboxRenderSystem     = skyboxRenderSystem.get(),
                .gridRenderSystem       = gridRenderSystem.get(),
                .deferredLightingSystem = deferredLightingSystem.get(),
                .postProcessingSystem   = postProcessingSystem.get(),
                .iblSystem              = iblSystem.get(),
                .camera                 = cameraSystem.get(),
                .colliderDebug          = colliderDebugRenderSystem.get(),
                .renderContext          = renderContext,
                .showSkybox             = &showSkybox,
                .showGrid               = &showGrid,
                .showDebugObjects       = &showDebugObjects,
                .showColliderWireframes = &showColliderWireframes,
                .debugMode              = &debugMode,
                .morphTargetManager     = morphTargetManager.get(),
            };
        }

        [[nodiscard]] SceneRuntimeStateView sceneState() {
            return SceneRuntimeStateView{
                .scene          = &scene,
                .selectedEntity = &selectedEntity,
                .cameraEntity   = &cameraEntity,
                .skybox         = skybox.get(),
                .skySettings    = &skySettings,
                .shadowSettings = &shadowSettings,
            };
        }

        [[nodiscard]] InputStateView inputState() {
            return InputStateView{
                .keyboard              = keyboard.get(),
                .mouse                 = mouse.get(),
                .inputSystem           = inputSystem.get(),
                .objectSelectionSystem = objectSelectionSystem.get(),
                .cameraSystem          = cameraSystem.get(),
            };
        }

        [[nodiscard]] ResourceStateView resourceState() {
            return ResourceStateView{
                .resourceManager         = resourceManager,
                .renderContext           = renderContext,
                .gbufferPool             = gbufferPool.get(),
                .gbufferSetLayout        = gbufferSetLayout.get(),
                .deferredIblPool         = deferredIblPool.get(),
                .deferredIblSetLayout    = deferredIblSetLayout.get(),
                .deferredShadowPool      = deferredShadowPool.get(),
                .deferredShadowSetLayout = deferredShadowSetLayout.get(),
                .postProcessPool         = postProcessPool.get(),
                .postProcessSetLayout    = postProcessSetLayout.get(),
            };
        }

       public:

        [[nodiscard]] RenderingStateService renderingService() {
            return RenderingStateService{*this};
        }

        [[nodiscard]] SceneRuntimeService sceneRuntimeService() {
            return SceneRuntimeService{*this};
        }

        [[nodiscard]] InputStateService inputService() {
            return InputStateService{*this};
        }

        [[nodiscard]] ResourceStateService resourceService() {
            return ResourceStateService{*this};
        }

        [[nodiscard]] AnimationRuntimeService animationRuntimeService() {
            return AnimationRuntimeService{*this};
        }

        [[nodiscard]] PhysicsRuntimeService physicsRuntimeService() {
            return PhysicsRuntimeService{*this};
        }

        [[nodiscard]] const std::vector<std::string>& initializedSystemOrder() const {
            return systemRegistry.initializationOrder();
        }

        [[nodiscard]] std::unique_ptr<Skybox>& skyboxRef() {
            return skybox;
        }

        [[nodiscard]] SkyboxSettings& skySettingsRef() {
            return skySettings;
        }

        [[nodiscard]] ShadowSettings& shadowSettingsRef() {
            return shadowSettings;
        }

        [[nodiscard]] PostProcessPushConstants& postProcessPushRef() {
            return postProcessPush;
        }

        [[nodiscard]] bool& showSkyboxRef() {
            return showSkybox;
        }

        [[nodiscard]] bool& showGridRef() {
            return showGrid;
        }

        [[nodiscard]] bool& showDebugObjectsRef() {
            return showDebugObjects;
        }

        [[nodiscard]] bool& showColliderWireframesRef() {
            return showColliderWireframes;
        }

        [[nodiscard]] bool& physicsSimulationRunningRef() {
            return physicsSimulationRunning;
        }

        [[nodiscard]] bool& solidGroundEnabledRef() {
            return solidGroundEnabled;
        }

        [[nodiscard]] JoltPhysicsSystem* getJoltPhysicsSystem() const {
            return joltPhysicsSystem.get();
        }

        void setPostProcessingSystem(std::unique_ptr<PostProcessingSystem> system) {
            postProcessingSystem = std::move(system);
        }

        [[nodiscard]] VkDescriptorSet getGbufferDescriptorSet(int frameIndex) const {
            if (frameIndex < 0 || frameIndex >= static_cast<int>(gbufferDescriptorSets.size())) {
                return VK_NULL_HANDLE;
            }
            return gbufferDescriptorSets[frameIndex];
        }

        [[nodiscard]] VkDescriptorSet& gbufferDescriptorSetRef(int frameIndex) {
            return gbufferDescriptorSets.at(static_cast<size_t>(frameIndex));
        }

        [[nodiscard]] VkDescriptorSet getDeferredIblDescriptorSet(int frameIndex) const {
            if (frameIndex < 0 || frameIndex >= static_cast<int>(deferredIblDescriptorSets.size())) {
                return VK_NULL_HANDLE;
            }
            return deferredIblDescriptorSets[frameIndex];
        }

        [[nodiscard]] VkDescriptorSet getDeferredShadowDescriptorSet(int frameIndex) const {
            if (frameIndex < 0 || frameIndex >= static_cast<int>(deferredShadowDescriptorSets.size())) {
                return VK_NULL_HANDLE;
            }
            return deferredShadowDescriptorSets[frameIndex];
        }

        [[nodiscard]] VkDescriptorSet& deferredShadowDescriptorSetRef(int frameIndex) {
            return deferredShadowDescriptorSets.at(static_cast<size_t>(frameIndex));
        }

        [[nodiscard]] VkDescriptorSet getPostProcessDescriptorSet(int frameIndex) const {
            if (frameIndex < 0 || frameIndex >= static_cast<int>(postProcessDescriptorSets.size())) {
                return VK_NULL_HANDLE;
            }
            return postProcessDescriptorSets[frameIndex];
        }

        [[nodiscard]] VkDescriptorSet& postProcessDescriptorSetRef(int frameIndex) {
            return postProcessDescriptorSets.at(static_cast<size_t>(frameIndex));
        }

        [[nodiscard]] std::vector<VkDescriptorSet>& deferredIblDescriptorSetsRef() {
            return deferredIblDescriptorSets;
        }

        [[nodiscard]] DescriptorSetLayout& gbufferSetLayoutRef() {
            return *gbufferSetLayout;
        }

        [[nodiscard]] DescriptorPool& gbufferPoolRef() {
            return *gbufferPool;
        }

        [[nodiscard]] DescriptorSetLayout& postProcessSetLayoutRef() {
            return *postProcessSetLayout;
        }

        [[nodiscard]] DescriptorPool& postProcessPoolRef() {
            return *postProcessPool;
        }

        [[nodiscard]] DescriptorSetLayout& deferredIblSetLayoutRef() {
            return *deferredIblSetLayout;
        }

        [[nodiscard]] DescriptorPool& deferredIblPoolRef() {
            return *deferredIblPool;
        }

       private:
        // initialization helpers - keep initialize() high-level and explicit
        void createInputDevices(Window* window);
        void initCoreSystems(Device& device, Renderer& renderer, bool multithreadedRecordingEnabled, uint32_t multithreadedRecordingThreads);
        void initDescriptorResources(Device& device, Renderer& renderer);
        void allocatePerFrameDescriptorSets(Renderer& renderer);
        void initPostProcessing(Device& device, Renderer& renderer);
        void initInputRelatedSystems(Window* window);

        // System registration functions for better readability
        bool registerCoreSystems(std::string& error);
        bool registerDescriptorResources(std::string& error);
        bool registerPerFrameDescriptors(std::string& error);
        bool registerPipelineLinks(std::string& error);
        bool registerPostProcessing(std::string& error);
        bool registerInputSystems(std::string& error);

        [[nodiscard]] SystemServicesView systemServices() const {
            return SystemServicesView{
                .objectSelection = objectSelectionSystem.get(),
                .input = inputSystem.get(),
                .camera = cameraSystem.get(),
                .colliderDebug = colliderDebugRenderSystem.get(),
                .animation = animationSystem.get(),
                .lod = lodSystem.get(),
                .modelRender = modelRenderSystem.get(),
                .shadow = shadowSystem.get(),
                .light = lightSystem.get(),
                .skyboxRender = skyboxRenderSystem.get(),
                .gridRender = gridRenderSystem.get(),
                .deferredLighting = deferredLightingSystem.get(),
                .postProcessing = postProcessingSystem.get(),
                .ibl = iblSystem.get(),
                .physics = physicsSystem.get(),
                .joltPhysics = joltPhysicsSystem.get(),
            };
        }

        SystemRegistry systemRegistry;

        // Systems
        std::unique_ptr<ObjectSelectionSystem>  objectSelectionSystem;
        std::unique_ptr<InputSystem>            inputSystem;
        std::unique_ptr<CameraSystem>           cameraSystem;
        std::unique_ptr<ColliderDebugRenderSystem> colliderDebugRenderSystem;
        std::unique_ptr<AnimationSystem>        animationSystem;
        std::unique_ptr<LODSystem>              lodSystem;
        std::unique_ptr<ModelRenderSystem>      modelRenderSystem;
        std::unique_ptr<ShadowSystem>           shadowSystem;
        std::unique_ptr<LightSystem>            lightSystem;
        std::unique_ptr<SkyboxRenderSystem>     skyboxRenderSystem;
        std::unique_ptr<GridRenderSystem>       gridRenderSystem;
        std::unique_ptr<DeferredLightingSystem> deferredLightingSystem;
        std::unique_ptr<PostProcessingSystem>   postProcessingSystem;
        std::unique_ptr<IBLSystem>              iblSystem;
        std::unique_ptr<PhysicsSystem>          physicsSystem;
        std::unique_ptr<JoltPhysicsSystem>      joltPhysicsSystem;
        std::unique_ptr<MorphTargetManager>     morphTargetManager;

        // Input devices (owned by EngineState)
        std::unique_ptr<Keyboard> keyboard;
        std::unique_ptr<Mouse>    mouse;

        // Descriptor/layout state used by several passes
        std::unique_ptr<DescriptorPool>      gbufferPool;
        std::unique_ptr<DescriptorSetLayout> gbufferSetLayout;
        std::vector<VkDescriptorSet>         gbufferDescriptorSets;

        std::unique_ptr<DescriptorPool>      deferredIblPool;
        std::unique_ptr<DescriptorSetLayout> deferredIblSetLayout;
        std::vector<VkDescriptorSet>         deferredIblDescriptorSets;

        std::unique_ptr<DescriptorPool>      deferredShadowPool;
        std::unique_ptr<DescriptorSetLayout> deferredShadowSetLayout;
        std::vector<VkDescriptorSet>         deferredShadowDescriptorSets;

        std::unique_ptr<DescriptorPool>      postProcessPool;
        std::unique_ptr<DescriptorSetLayout> postProcessSetLayout;
        std::vector<VkDescriptorSet>         postProcessDescriptorSets;

        // Non-owned dependencies passed during initialize().
        RenderContext*   renderContext   = nullptr;
        ResourceManager* resourceManager = nullptr;

        // Scene & transient selection state.
        Scene        scene;
        entt::entity selectedEntity = entt::null;
        entt::entity cameraEntity   = entt::null;

        // Scene resources and runtime rendering controls.
        std::unique_ptr<Skybox> skybox;
        SkyboxSettings          skySettings;
        ShadowSettings          shadowSettings;
        PostProcessPushConstants postProcessPush{};

        bool showSkybox             = false;
        bool showGrid               = false;
        bool showDebugObjects       = false;
        bool showColliderWireframes = false;
        bool debugMode              = false;
        bool physicsSimulationRunning = false;
        bool solidGroundEnabled       = true;
    };

}  // namespace engine
