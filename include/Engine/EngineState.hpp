#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/SystemRegistry.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "Editor/RenderContext.hpp"
#include "Editor/ui/UIManager.hpp"
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
    class EngineState {
       public:
        struct RenderingState {
            ModelRenderSystem*      modelRenderSystem      = nullptr;
            ShadowSystem*           shadowSystem           = nullptr;
            LightSystem*            lightSystem            = nullptr;
            SkyboxRenderSystem*     skyboxRenderSystem     = nullptr;
            GridRenderSystem*       gridRenderSystem       = nullptr;
            DustRenderSystem*       dustRenderSystem       = nullptr;
            DeferredLightingSystem* deferredLightingSystem = nullptr;
            PostProcessingSystem*   postProcessingSystem   = nullptr;
            IBLSystem*              iblSystem              = nullptr;
            RenderContext*          renderContext          = nullptr;
            bool*                   showSkybox             = nullptr;
            bool*                   showGrid               = nullptr;
            bool*                   debugMode              = nullptr;
        };

        struct SceneState {
            Scene*          scene          = nullptr;
            entt::entity*   selectedEntity = nullptr;
            entt::entity*   cameraEntity   = nullptr;
            Skybox*         skybox         = nullptr;
            SkyboxSettings* skySettings    = nullptr;
            DustSettings*   dustSettings   = nullptr;
            FogSettings*    fogSettings    = nullptr;
            HZBSettings*    hzbSettings    = nullptr;
            ShadowSettings* shadowSettings = nullptr;
        };

        struct InputState {
            Keyboard*              keyboard              = nullptr;
            Mouse*                 mouse                 = nullptr;
            InputSystem*           inputSystem           = nullptr;
            ObjectSelectionSystem* objectSelectionSystem = nullptr;
            CameraSystem*          cameraSystem          = nullptr;
        };

        struct ResourceState {
            ResourceManager*     resourceManager         = nullptr;
            RenderContext*       renderContext           = nullptr;
            DescriptorPool*      gbufferPool             = nullptr;
            DescriptorSetLayout* gbufferSetLayout        = nullptr;
            DescriptorPool*      deferredIblPool         = nullptr;
            DescriptorSetLayout* deferredIblSetLayout    = nullptr;
            DescriptorPool*      deferredShadowPool      = nullptr;
            DescriptorSetLayout* deferredShadowSetLayout = nullptr;
            DescriptorPool*      postProcessPool         = nullptr;
            DescriptorSetLayout* postProcessSetLayout    = nullptr;
        };

        // lifecycle
        void initialize(Device& device,
            Renderer&           renderer,
            ResourceManager&    resourceManager,
            Window*             window,
            bool                multithreadedRecordingEnabled,
            uint32_t            multithreadedRecordingThreads);

        [[nodiscard]] RenderingState renderingState() {
            return RenderingState{
                .modelRenderSystem      = modelRenderSystem.get(),
                .shadowSystem           = shadowSystem.get(),
                .lightSystem            = lightSystem.get(),
                .skyboxRenderSystem     = skyboxRenderSystem.get(),
                .gridRenderSystem       = gridRenderSystem.get(),
                .dustRenderSystem       = dustRenderSystem.get(),
                .deferredLightingSystem = deferredLightingSystem.get(),
                .postProcessingSystem   = postProcessingSystem.get(),
                .iblSystem              = iblSystem.get(),
                .renderContext          = renderContext.get(),
                .showSkybox             = &showSkybox,
                .showGrid               = &showGrid,
                .debugMode              = &debugMode,
            };
        }

        [[nodiscard]] SceneState sceneState() {
            return SceneState{
                .scene          = &scene,
                .selectedEntity = &selectedEntity,
                .cameraEntity   = &cameraEntity,
                .skybox         = skybox.get(),
                .skySettings    = &skySettings,
                .dustSettings   = &dustSettings,
                .fogSettings    = &fogSettings,
                .hzbSettings    = &hzbSettings,
                .shadowSettings = &shadowSettings,
            };
        }

        [[nodiscard]] InputState inputState() {
            return InputState{
                .keyboard              = keyboard.get(),
                .mouse                 = mouse.get(),
                .inputSystem           = inputSystem.get(),
                .objectSelectionSystem = objectSelectionSystem.get(),
                .cameraSystem          = cameraSystem.get(),
            };
        }

        [[nodiscard]] ResourceState resourceState() {
            return ResourceState{
                .resourceManager         = resourceManager,
                .renderContext           = renderContext.get(),
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

        [[nodiscard]] const std::vector<std::string>& initializedSystemOrder() const {
            return systemRegistry.initializationOrder();
        }

        [[nodiscard]] Scene& getScene() {
            return scene;
        }

        [[nodiscard]] const Scene& getScene() const {
            return scene;
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

        SystemRegistry systemRegistry;

       public:
        // Systems
        std::unique_ptr<ObjectSelectionSystem>  objectSelectionSystem;
        std::unique_ptr<InputSystem>            inputSystem;
        std::unique_ptr<CameraSystem>           cameraSystem;
        std::unique_ptr<AnimationSystem>        animationSystem;
        std::unique_ptr<LODSystem>              lodSystem;
        std::unique_ptr<ModelRenderSystem>      modelRenderSystem;
        std::unique_ptr<ShadowSystem>           shadowSystem;
        std::unique_ptr<LightSystem>            lightSystem;
        std::unique_ptr<SkyboxRenderSystem>     skyboxRenderSystem;
        std::unique_ptr<GridRenderSystem>       gridRenderSystem;
        std::unique_ptr<DustRenderSystem>       dustRenderSystem;
        std::unique_ptr<DeferredLightingSystem> deferredLightingSystem;
        std::unique_ptr<PostProcessingSystem>   postProcessingSystem;
        std::unique_ptr<IBLSystem>              iblSystem;

        // Resources
        std::unique_ptr<RenderContext> renderContext;
        ResourceManager*               resourceManager = nullptr;  // not owned here

        // Input devices (owned by EngineState)
        std::unique_ptr<Keyboard> keyboard;
        std::unique_ptr<Mouse>    mouse;

        // Scene & entities
        Scene        scene;
        entt::entity selectedEntity = entt::null;
        entt::entity cameraEntity   = entt::null;

        // UI
        std::unique_ptr<UIManager>    uiManager;
        std::unique_ptr<ImGuiManager> imguiManager;

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
        PostProcessPushConstants             postProcessPush{};

        // Scene resources
        std::unique_ptr<Skybox> skybox;
        SkyboxSettings          skySettings;
        DustSettings            dustSettings;
        FogSettings             fogSettings;
        HZBSettings             hzbSettings;
        ShadowSettings          shadowSettings;

        // View toggles
        bool showSkybox = false;
        bool showGrid   = false;
        bool debugMode  = false;
    };

}  // namespace engine
