#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/DescriptorManager.hpp"
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
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/SelectionOutlineSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    class Device;
    class Renderer;
    class Keyboard;
    class Mouse;
    class Window;

    // EngineState is the single source-of-truth for owned systems, scene and
    // runtime settings. Pass a pointer/reference to render passes and systems
    // so they can access the current runtime state without long parameter lists.
    class EngineFacade;
    class DescriptorManager;

    class EngineState {
       public:
        ~EngineState();

        // lifecycle
        void initialize(Device& device,
            Renderer&           renderer,
            ResourceManager&    resourceManager,
            IRenderContextPort* requiredRenderContextPort,
            Window*             window,
            bool                multithreadedRecordingEnabled,
            uint32_t            multithreadedRecordingThreads);

       private:
        friend class EngineFacade;
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
                .selectionOutline       = selectionOutlineSystem.get(),
                .renderContextPort      = renderContextPort,
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
                .renderContextPort       = renderContextPort,
                .gbufferPool             = &descriptorManager->gbufferPool(),
                .gbufferSetLayout        = &descriptorManager->gbufferSetLayout(),
                .deferredIblPool         = &descriptorManager->deferredIblPool(),
                .deferredIblSetLayout    = &descriptorManager->deferredIblSetLayout(),
                .deferredShadowPool      = &descriptorManager->deferredShadowPool(),
                .deferredShadowSetLayout = &descriptorManager->deferredShadowSetLayout(),
                .postProcessPool         = &descriptorManager->postProcessPool(),
                .postProcessSetLayout    = &descriptorManager->postProcessSetLayout(),
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

        [[nodiscard]] Scene& sceneRef() {
            return scene;
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

        [[nodiscard]] entt::entity& selectedEntityRef() {
            return selectedEntity;
        }

        [[nodiscard]] entt::entity& cameraEntityRef() {
            return cameraEntity;
        }

        [[nodiscard]] JoltPhysicsSystem* getJoltPhysicsSystem() const {
            return joltPhysicsSystem.get();
        }

        void setPostProcessingSystem(std::unique_ptr<PostProcessingSystem> system) {
            postProcessingSystem = std::move(system);
        }

        [[nodiscard]] VkDescriptorSet getGbufferDescriptorSet(int frameIndex) const {
            return descriptorManager->gbufferDescriptorSet(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet& gbufferDescriptorSetRef(int frameIndex) {
            return descriptorManager->gbufferDescriptorSetRef(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet getDeferredIblDescriptorSet(int frameIndex) const {
            return descriptorManager->deferredIblDescriptorSet(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet getDeferredShadowDescriptorSet(int frameIndex) const {
            return descriptorManager->deferredShadowDescriptorSet(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet& deferredShadowDescriptorSetRef(int frameIndex) {
            return descriptorManager->deferredShadowDescriptorSetRef(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet getPostProcessDescriptorSet(int frameIndex) const {
            return descriptorManager->postProcessDescriptorSet(frameIndex);
        }

        [[nodiscard]] VkDescriptorSet& postProcessDescriptorSetRef(int frameIndex) {
            return descriptorManager->postProcessDescriptorSetRef(frameIndex);
        }

        [[nodiscard]] std::vector<VkDescriptorSet>& deferredIblDescriptorSetsRef() {
            return descriptorManager->deferredIblDescriptorSets();
        }

        [[nodiscard]] DescriptorSetLayout& gbufferSetLayoutRef() {
            return descriptorManager->gbufferSetLayout();
        }

        [[nodiscard]] DescriptorPool& gbufferPoolRef() {
            return descriptorManager->gbufferPool();
        }

        [[nodiscard]] DescriptorSetLayout& postProcessSetLayoutRef() {
            return descriptorManager->postProcessSetLayout();
        }

        [[nodiscard]] DescriptorPool& postProcessPoolRef() {
            return descriptorManager->postProcessPool();
        }

        [[nodiscard]] DescriptorSetLayout& deferredIblSetLayoutRef() {
            return descriptorManager->deferredIblSetLayout();
        }

        [[nodiscard]] DescriptorPool& deferredIblPoolRef() {
            return descriptorManager->deferredIblPool();
        }

        [[nodiscard]] DescriptorSetLayout& deferredShadowSetLayoutRef() {
            return descriptorManager->deferredShadowSetLayout();
        }

        [[nodiscard]] DescriptorPool& deferredShadowPoolRef() {
            return descriptorManager->deferredShadowPool();
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
                .objectSelection  = objectSelectionSystem.get(),
                .input            = inputSystem.get(),
                .camera           = cameraSystem.get(),
                .colliderDebug    = colliderDebugRenderSystem.get(),
                .selectionOutline = selectionOutlineSystem.get(),
                .animation        = animationSystem.get(),
                .lod              = lodSystem.get(),
                .modelRender      = modelRenderSystem.get(),
                .shadow           = shadowSystem.get(),
                .light            = lightSystem.get(),
                .skyboxRender     = skyboxRenderSystem.get(),
                .gridRender       = gridRenderSystem.get(),
                .deferredLighting = deferredLightingSystem.get(),
                .postProcessing   = postProcessingSystem.get(),
                .ibl              = iblSystem.get(),
                .physics          = physicsSystem.get(),
                .joltPhysics      = joltPhysicsSystem.get(),
            };
        }

        SystemRegistry systemRegistry;

        // Systems
        std::unique_ptr<ObjectSelectionSystem>     objectSelectionSystem;
        std::unique_ptr<InputSystem>               inputSystem;
        std::unique_ptr<CameraSystem>              cameraSystem;
        std::unique_ptr<ColliderDebugRenderSystem> colliderDebugRenderSystem;
        std::unique_ptr<SelectionOutlineSystem>    selectionOutlineSystem;
        std::unique_ptr<AnimationSystem>           animationSystem;
        std::unique_ptr<LODSystem>                 lodSystem;
        std::unique_ptr<ModelRenderSystem>         modelRenderSystem;
        std::unique_ptr<ShadowSystem>              shadowSystem;
        std::unique_ptr<LightSystem>               lightSystem;
        std::unique_ptr<SkyboxRenderSystem>        skyboxRenderSystem;
        std::unique_ptr<GridRenderSystem>          gridRenderSystem;
        std::unique_ptr<DeferredLightingSystem>    deferredLightingSystem;
        std::unique_ptr<PostProcessingSystem>      postProcessingSystem;
        std::unique_ptr<IBLSystem>                 iblSystem;
        std::unique_ptr<PhysicsSystem>             physicsSystem;
        std::unique_ptr<JoltPhysicsSystem>         joltPhysicsSystem;
        std::unique_ptr<MorphTargetManager>        morphTargetManager;

        // Input devices (owned by EngineState)
        std::unique_ptr<Keyboard> keyboard;
        std::unique_ptr<Mouse>    mouse;

        // Descriptor/layout state — centralized in DescriptorManager.
        std::unique_ptr<class DescriptorManager> descriptorManager;

        // Non-owned dependencies passed during initialize().
        IRenderContextPort* renderContextPort = nullptr;
        ResourceManager*    resourceManager   = nullptr;

        // Scene & transient selection state.
        Scene        scene;
        entt::entity selectedEntity = entt::null;
        entt::entity cameraEntity   = entt::null;

        // Scene resources and runtime rendering controls.
        std::unique_ptr<Skybox>  skybox;
        SkyboxSettings           skySettings;
        ShadowSettings           shadowSettings;
        PostProcessPushConstants postProcessPush{};

        bool showSkybox               = false;
        bool showGrid                 = false;
        bool showDebugObjects         = false;
        bool showColliderWireframes   = false;
        bool debugMode                = false;
        bool physicsSimulationRunning = false;
        bool solidGroundEnabled       = true;
    };

}  // namespace engine
