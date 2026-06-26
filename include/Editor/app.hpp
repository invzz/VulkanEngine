#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <memory>

#include "Engine/Application/Ports/IEnvironmentLightingPort.hpp"
#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"
#include "Engine/Application/Ports/IScenePersistencePort.hpp"
#include "Engine/Application/Ports/ISceneSelectionMaintenancePort.hpp"
#include "Engine/Application/SceneRuntimeState.hpp"
#include "Engine/Application/UseCases/CameraManagementUseCase.hpp"
#include "Engine/Application/UseCases/LoadSceneUseCase.hpp"
#include "Engine/Application/UseCases/ProcessSceneSelectionMaintenanceUseCase.hpp"
#include "Engine/Application/UseCases/ReconcileSceneLoadUseCase.hpp"
#include "Engine/Application/UseCases/SaveSceneUseCase.hpp"
#include "Engine/Application/UseCases/SceneEntityManagementUseCase.hpp"
#include "Engine/Application/UseCases/SceneSettingsManagementUseCase.hpp"
#include "Engine/Application/UseCases/SyncEnvironmentLightingUseCase.hpp"
#include "Engine/Application/UseCases/TransformManipulationUseCase.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/EngineFacade.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/RenderPipeline.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/ViewportTexture.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "Editor/Infrastructure/AnimationAccessAdapter.hpp"
#include "Editor/Infrastructure/CompositionAdapter.hpp"
#include "Editor/Infrastructure/DescriptorAccessAdapter.hpp"
#include "Editor/Infrastructure/PostProcessingAccessAdapter.hpp"
#include "Editor/Infrastructure/RenderContextAdapter.hpp"
#include "Editor/Infrastructure/RuntimeStateAdapter.hpp"
#include "Editor/Infrastructure/SceneRuntimeAccessAdapter.hpp"
#include "Engine/Graphics/ViewportDisplay.hpp"

#include "Editor/ui/ViewportPanel.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    // Forward declarations
    class AnimationSystem;
    class CameraSystem;
    class InputSystem;
    class ObjectSelectionSystem;
    class ModelRenderSystem;
    class LightSystem;
    class RenderContext;
    class ShadowSystem;
    class LODSystem;
    class UIManager;
    class Camera;
    class EngineFacade;

    class IBLSystem;
    class ImGuiManager;
    class RenderGraph;
    class GridRenderSystem;

    struct GameLoopState {
        ObjectSelectionSystem& objectSelectionSystem;
        InputSystem&           inputSystem;
        CameraSystem&          cameraSystem;
        AnimationSystem&       animationSystem;
        LODSystem&             lodSystem;
        ModelRenderSystem&     modelRenderSystem;
        LightSystem&           lightSystem;
        ShadowSystem&          shadowSystem;
        SkyboxRenderSystem&    skyboxRenderSystem;
        GridRenderSystem&      gridRenderSystem;
        RenderContext&         renderContext;
        UIManager&             uiManager;
        Skybox*                skybox;
        bool                   showGrid;
        SkyboxSettings&        skySettings;
        ShadowSettings&        shadowSettings;
    };

    class App {
       public:
        static int width() {
            return 800;
        }
        static int height() {
            return 600;
        }

        App(bool fullscreen = false);
        ~App();

        // delete copy operations
        App(const App&)            = delete;
        App& operator=(const App&) = delete;

        void run();

       private:
        void              init();
        void              setupSystems();
        void              setupScene();
        void              setupUI();
        void              setupRenderGraph();
        SceneRuntimeState sceneRuntimeState();

        void update(float frameTime);
        void render(float frameTime);

        Window          window;
        Device          device{window};
        Renderer        renderer{window, device};
        ResourceManager resourceManager{device};
        int             debugMode = 0;

        // Central engine state (owns systems, scene, resources used by passes)
        EngineState                   engineState;
        SceneSerializer               sceneSerializer;  // initialized in ctor using engineState
        std::unique_ptr<EngineFacade> engineFacade;     // facade over engineState — lifetime must outlive render graph

        // Explicitly owned by App and injected into EngineState during initialize().
        std::unique_ptr<RenderContext> renderContext;
        std::unique_ptr<ImGuiManager>  imguiManager;
        std::unique_ptr<UIManager>     uiManager;

        // Clean architecture wiring (Delivery -> Application via Ports).
        std::unique_ptr<IScenePersistencePort>                   scenePersistencePort;
        std::unique_ptr<IPhysicsRuntimePort>                     physicsRuntimePort;
        std::unique_ptr<ISceneSelectionMaintenancePort>          sceneSelectionMaintenancePort;
        std::unique_ptr<IEnvironmentLightingPort>                environmentLightingPort;
        std::unique_ptr<ISceneEntityPort>                        sceneEntityPort;
        std::unique_ptr<ICameraPort>                             cameraPort;
        std::unique_ptr<ITransformPort>                          transformPort;
        std::unique_ptr<ISceneSettingsPort>                      sceneSettingsPort;
        std::unique_ptr<LoadSceneUseCase>                        loadSceneUseCase;
        std::unique_ptr<ProcessSceneSelectionMaintenanceUseCase> processSceneSelectionMaintenanceUseCase;
        std::unique_ptr<ReconcileSceneLoadUseCase>               reconcileSceneLoadUseCase;
        std::unique_ptr<SaveSceneUseCase>                        saveSceneUseCase;
        std::unique_ptr<SyncEnvironmentLightingUseCase>          syncEnvironmentLightingUseCase;
        std::unique_ptr<SceneEntityManagementUseCase>            sceneEntityManagementUseCase;
        std::unique_ptr<CameraManagementUseCase>                 cameraManagementUseCase;
        std::unique_ptr<TransformManipulationUseCase>            transformManipulationUseCase;
        std::unique_ptr<SceneSettingsManagementUseCase>          sceneSettingsManagementUseCase;

        // Infrastructure adapters for render pass state views.
        std::unique_ptr<DescriptorAccessAdapter>     descriptorAccessAdapter;
        std::unique_ptr<RuntimeStateAdapter>         runtimeStateAdapter;
        std::unique_ptr<AnimationAccessAdapter>      animationAccessAdapter;
        std::unique_ptr<RenderContextAdapter>        renderContextAdapter;
        std::unique_ptr<CompositionAdapter>          compositionAdapter;
        std::unique_ptr<SceneRuntimeAccessAdapter>   sceneRuntimeAccessAdapter;
        std::unique_ptr<PostProcessingAccessAdapter> postProcessingAccessAdapter;

        // Input & Camera
        std::unique_ptr<Camera> camera;

        // Demo control: multithreaded secondary-command-buffer recording (opt-in pilot).
        // Uses thread-local command pools and secondary command buffers for G-buffer recording.
        bool     multithreadedRecordingEnabled = true;
        uint32_t multithreadedRecordingThreads = 0;

        // Render Graph
        std::unique_ptr<RenderPipeline> renderPipeline;

        // Viewport rendering
        ViewportTexture                viewportTexture_;
        std::unique_ptr<ViewportPanel> viewportPanel_;
        bool                           viewportTextureInitialized_ = false;

        // Viewport display (renders HDR viewport texture to swap chain)
        ViewportDisplay viewportDisplay_;

        uint32_t selectedObjectId = 0;

        bool pendingUpdateCameraAfterSceneLoad = false;
    };
}  // namespace engine

#endif  // EDITOR_APP_HPP
