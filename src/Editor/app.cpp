#include "Editor/app.hpp"

#include <glm/common.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/TextureManager.hpp"
#include "glm/ext/vector_float3.hpp"
#include "vulkan/vulkan_core.h"

// Systems
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

// Render Passes
#include "Engine/Graphics/Passes/CompositionPass.hpp"
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Graphics/Passes/DepthPrepass.hpp"
#include "Engine/Graphics/Passes/OffscreenPass.hpp"
#include "Engine/Graphics/Passes/ShadowPass.hpp"
#include "Engine/Graphics/Passes/UpdatePass.hpp"

// Demo specific
#include "Engine/EngineFacade.hpp"

#include "Editor/Infrastructure/CameraAdapter.hpp"
#include "Editor/Infrastructure/EnvironmentLightingAdapter.hpp"
#include "Editor/Infrastructure/PhysicsRuntimeAdapter.hpp"
#include "Editor/Infrastructure/SceneEntityAdapter.hpp"
#include "Editor/Infrastructure/ScenePersistenceAdapter.hpp"
#include "Editor/Infrastructure/SceneSettingsAdapter.hpp"
#include "Editor/Infrastructure/TransformAdapter.hpp"
#include "Editor/RenderContext.hpp"

// UI Panels
#include "Editor/ui/AnimationPanel.hpp"
#include "Editor/ui/InspectorPanel.hpp"
#include "Editor/ui/PhysicsPanel.hpp"
#include "Editor/ui/Scene/ScenePanel.hpp"
#include "Editor/ui/SettingsPanel.hpp"
#include "Editor/ui/ToolbarPanel.hpp"
#include "Editor/ui/UIManager.hpp"

namespace engine {

    namespace {

        class SceneSelectionMaintenanceAdapter final : public ISceneSelectionMaintenancePort {
           public:
            explicit SceneSelectionMaintenanceAdapter(UIManager& uiManager)
                : uiManager_(uiManager) {}

            void processSelectionMaintenance(SceneRuntimeState& runtimeState) override {
                if (auto* scenePanel = uiManager_.getPanel<ScenePanel>()) {
                    scenePanel->processDelayedDeletions(runtimeState.selectedEntity, runtimeState.selectedObjectId);
                }
            }

           private:
            UIManager& uiManager_;
        };

    }  // namespace

    App::App(bool fullscreen)
        : window(width(), height(), "Vulkan Editor", fullscreen), device(window), renderer(window, device), resourceManager(device), sceneSerializer(*engineState.sceneRuntimeService().view().scene, resourceManager) {
        init();
    }

    App::~App() {
        try {
            resourceManager.waitForAsyncLoads();
            resourceManager.updateAsyncCallbacks();
            device.WaitIdle();
        } catch (const std::exception& e) {
            std::cerr << "[App::~App] Shutdown drain failed: " << e.what() << '\n';
        } catch (...) {
            std::cerr << "[App::~App] Shutdown drain failed with unknown exception\n";
        }

        GpuProfiler::instance().shutdown();
    }

    void App::init() {
        // Enable thread-local command pools BEFORE any system uses multithreaded
        // command buffer recording. This avoids VkCommandPool threading validation
        // errors when worker threads allocate secondary command buffers while the
        // main thread performs single-time commands (e.g., texture uploads).
        device.enableThreadLocalCommandPools();

        // 1. Setup explicit RenderContext dependency for EngineState.
        renderContext        = std::make_unique<RenderContext>(device, resourceManager.getMeshManager());
        renderContextAdapter = std::make_unique<RenderContextAdapter>(renderContext.get());

        // 2. Setup Scene & Camera
        setupScene();

        // 3. Initialize centralized EngineState (systems, descriptors, pools, post-process)
        engineState.initialize(device, renderer, resourceManager, renderContextAdapter.get(), &window, multithreadedRecordingEnabled, multithreadedRecordingThreads);
        engineFacade = std::make_unique<EngineFacade>(engineState);

        auto rendering    = engineState.renderingService().view();
        auto sceneRuntime = engineState.sceneRuntimeService().view();

        // Wire infrastructure adapters for runtime state access.
        sceneRuntimeAccessAdapter   = std::make_unique<SceneRuntimeAccessAdapter>(engineState);
        postProcessingAccessAdapter = std::make_unique<PostProcessingAccessAdapter>(engineState);

        sceneSerializer.setRuntimeSettingsBindings(RuntimeSettingsBindings{
            .showSkybox                    = sceneRuntimeAccessAdapter->showSkybox(),
            .showGrid                      = sceneRuntimeAccessAdapter->showGrid(),
            .showDebugObjects              = sceneRuntimeAccessAdapter->showDebugObjects(),
            .physicsSimulationRunning      = sceneRuntimeAccessAdapter->physicsSimulationRunning(),
            .skySettings                   = sceneRuntimeAccessAdapter->skySettings(),
            .postProcessPush               = sceneRuntimeAccessAdapter->postProcessPush(),
            .iblSystem                     = rendering.iblSystem,
            .modelRenderSystem             = rendering.modelRenderSystem,
            .getGpuProfilerEnabled         = []() { return GpuProfiler::instance().isEnabled(); },
            .setGpuProfilerEnabled         = [](bool enabled) { GpuProfiler::instance().setEnabled(enabled); },
            .multithreadedRecordingEnabled = &multithreadedRecordingEnabled,
            .multithreadedRecordingThreads = &multithreadedRecordingThreads,
            .debugMode                     = &debugMode,
        });

        scenePersistencePort           = std::make_unique<ScenePersistenceAdapter>(sceneSerializer);
        physicsRuntimePort             = std::make_unique<PhysicsRuntimeAdapter>(engineState);
        environmentLightingPort        = std::make_unique<EnvironmentLightingAdapter>(device, engineState);
        sceneEntityPort                = std::make_unique<SceneEntityAdapter>(engineState);
        cameraPort                     = std::make_unique<CameraAdapter>(engineState);
        transformPort                  = std::make_unique<TransformAdapter>(engineState);
        sceneSettingsPort              = std::make_unique<SceneSettingsAdapter>(engineState);
        loadSceneUseCase               = std::make_unique<LoadSceneUseCase>(*sceneRuntime.scene, *scenePersistencePort, physicsRuntimePort.get());
        reconcileSceneLoadUseCase      = std::make_unique<ReconcileSceneLoadUseCase>(*sceneRuntime.scene);
        saveSceneUseCase               = std::make_unique<SaveSceneUseCase>(*scenePersistencePort);
        syncEnvironmentLightingUseCase = std::make_unique<SyncEnvironmentLightingUseCase>(*environmentLightingPort);
        sceneEntityManagementUseCase   = std::make_unique<SceneEntityManagementUseCase>(*sceneEntityPort);
        cameraManagementUseCase        = std::make_unique<CameraManagementUseCase>(*cameraPort);
        transformManipulationUseCase   = std::make_unique<TransformManipulationUseCase>(*transformPort);
        sceneSettingsManagementUseCase = std::make_unique<SceneSettingsManagementUseCase>(*sceneSettingsPort, *physicsRuntimePort);

        // Infrastructure adapters for render pass state views.
        descriptorAccessAdapter = std::make_unique<DescriptorAccessAdapter>(engineState);
        runtimeStateAdapter     = std::make_unique<RuntimeStateAdapter>(engineState);

        // 4. Setup UI
        setupUI();

        sceneSelectionMaintenancePort           = std::make_unique<SceneSelectionMaintenanceAdapter>(*uiManager);
        processSceneSelectionMaintenanceUseCase = std::make_unique<ProcessSceneSelectionMaintenanceUseCase>(*sceneSelectionMaintenancePort);

        // If a scene file exists in the working directory, load it at startup
        if (std::filesystem::exists("scene.json")) {
            std::cout << "[App] Found scene.json, loading at startup..." << '\n';
            auto loadRefs = sceneRuntimeState();

            if (loadSceneUseCase->execute("scene.json", loadRefs)) {
                std::cout << "[App] Loaded scene.json at startup" << '\n';
            } else {
                std::cout << "[App] Failed to deserialize scene.json at startup" << '\n';
            }
        }

        // 5. Setup Render Graph
        renderPipeline = std::make_unique<RenderPipeline>(renderer);
        setupRenderGraph();

        (void) GpuProfiler::instance().initialize(
            device.device(),
            device.getProperties().limits.timestampPeriod,
            static_cast<uint32_t>(SwapChain::maxFramesInFlight()),
            32);

        // Profiling starts disabled by default and can be enabled from Settings.
        GpuProfiler::instance().setEnabled(false);
    }

    void App::setupScene() {
        camera                   = std::make_unique<Camera>();
        auto sceneState          = engineState.sceneRuntimeService().view();
        *sceneState.cameraEntity = sceneState.scene->createEntity();
        sceneState.scene->getRegistry().emplace<TransformComponent>(*sceneState.cameraEntity);
        sceneState.scene->getRegistry().emplace<NameComponent>(*sceneState.cameraEntity, "Camera");
        sceneState.scene->getRegistry().get<TransformComponent>(*sceneState.cameraEntity).translation = {0.0f, -0.2f, -2.5f};
        sceneState.scene->getRegistry().emplace<CameraComponent>(*sceneState.cameraEntity);
    }

    void App::setupSystems() {
        // All systems, descriptor pools and per-frame descriptor sets are now owned
        // and initialized by EngineState::initialize(). Nothing to do here.
        (void) device;
        (void) renderer;  // keep unused param silence free if needed
    }

    void App::setupUI() {
        imguiManager = std::make_unique<ImGuiManager>(window, device, renderer.getSwapChainRenderPass(), static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
        uiManager    = std::make_unique<UIManager>(*imguiManager);

        // Wire save/load callbacks through UIManager
        uiManager->setOnSaveScene([this]() {
            std::cout << "Saving scene to scene.json..." << '\n';
            if (!saveSceneUseCase->execute("scene.json")) {
                std::cout << "[App] Failed to serialize scene.json\n";
            }
        });
        uiManager->setOnLoadScene([this]() {
            std::cout << "Loading scene from scene.json..." << '\n';
            auto loadRefs = sceneRuntimeState();

            if (loadSceneUseCase->execute("scene.json", loadRefs)) {
                std::cout << "[App] Successfully deserialized scene.json\n";
            } else {
                std::cout << "[App] Failed to deserialize scene.json\n";
            }
        });

        // --- Register panels with docking constraints ---
        auto& registry = uiManager->getPanelRegistry();
        auto& state    = uiManager->getUIState();

        // Scene hierarchy panel — dock left
        auto scenePanel = std::make_unique<ScenePanel>(device, &engineState);
        registry.registerPanel("SceneHierarchy", std::move(scenePanel), DockConstraints{
                                                                            .preferredZone = DockZone::DockLeft,
                                                                            .minSizeX      = 250.0f,
                                                                            .minSizeY      = 200.0f,
                                                                        });

        // Inspector panel — dock right
        auto inspectorPanel = std::make_unique<InspectorPanel>(
            *sceneRuntimeAccessAdapter->scene(),
            sceneRuntimeAccessAdapter->physicsSimulationRunning(),
            sceneRuntimeAccessAdapter->showColliderWireframes(),
            sceneRuntimeAccessAdapter->solidGroundEnabled(),
            physicsRuntimePort->joltPhysicsSystem());
        registry.registerPanel("Inspector", std::move(inspectorPanel), DockConstraints{
                                                                           .preferredZone = DockZone::DockRight,
                                                                           .minSizeX      = 300.0f,
                                                                           .minSizeY      = 200.0f,
                                                                       });

        // Settings panel — dock bottom
        auto settingsPanel = std::make_unique<SettingsPanel>(&engineState, multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode);
        registry.registerPanel("Settings", std::move(settingsPanel), DockConstraints{
                                                                         .preferredZone = DockZone::DockBottom,
                                                                         .minSizeX      = 400.0f,
                                                                         .minSizeY      = 150.0f,
                                                                     });

        // Physics panel — dockable anywhere
        auto physicsPanel = std::make_unique<PhysicsPanel>(
            *sceneRuntimeAccessAdapter->scene(),
            sceneRuntimeAccessAdapter->physicsSimulationRunning(),
            sceneRuntimeAccessAdapter->showColliderWireframes(),
            sceneRuntimeAccessAdapter->solidGroundEnabled(),
            physicsRuntimePort->joltPhysicsSystem());
        registry.registerPanel("Physics", std::move(physicsPanel), DockConstraints{
                                                                       .preferredZone = DockZone::DockCenter,
                                                                       .minSizeX      = 300.0f,
                                                                       .minSizeY      = 200.0f,
                                                                   });

        // --- Toolbar panel ---
        auto toolbar = std::make_unique<ToolbarPanel>();
        uiManager->setToolbarPanel(std::move(toolbar));
        uiManager->addToolbarToggle("Scene", registry.getPanel("SceneHierarchy"));
        uiManager->addToolbarToggle("Inspector", registry.getPanel("Inspector"));
        uiManager->addToolbarToggle("Settings", registry.getPanel("Settings"));
        uiManager->addToolbarToggle("Physics", registry.getPanel("Physics"));
    }

    void App::setupRenderGraph() {
        auto graph = std::make_unique<RenderGraph>();

        // Create infrastructure adapters for render pass state views.
        descriptorAccessAdapter = std::make_unique<DescriptorAccessAdapter>(engineState);
        runtimeStateAdapter     = std::make_unique<RuntimeStateAdapter>(engineState);
        animationAccessAdapter  = std::make_unique<AnimationAccessAdapter>(engineState.animationRuntimeService().animation());
        compositionAdapter      = std::make_unique<CompositionAdapter>(engineState, uiManager.get());

        // Build state views from adapters for pass injection.
        RenderingStateView    renderingView    = engineState.renderingService().view();
        SceneRuntimeStateView sceneRuntimeView = engineState.sceneRuntimeService().view();
        InputStateView        inputView        = engineState.inputService().view();

        // EngineFacade — single accessor for all pass state needs.
        EngineFacade& engineFacadeRef = *engineFacade;

        // 1. Update Pass
        graph->addPass(std::make_unique<UpdatePass>(engineFacadeRef, physicsRuntimePort.get(), renderer));

        // 2. Compute Pass
        graph->addPass(std::make_unique<ComputePass>(animationAccessAdapter.get()));

        // 3. Shadow Pass (state views)
        graph->addPass(std::make_unique<ShadowPass>(renderingView, sceneRuntimeView, renderContextAdapter.get()));

        // 4. Depth Prepass (Offscreen Depth Only)
        graph->addPass(std::make_unique<DepthPrepass>(renderingView, renderer));

        // 5. Offscreen Pass (Main Scene - Load depth from prepass)
        graph->addPass(std::make_unique<OffscreenPass>(renderer, renderingView, *descriptorAccessAdapter, *runtimeStateAdapter, device, debugMode));

        // 6. Composition Pass (PostProcess + UI)
        graph->addPass(std::make_unique<CompositionPass>(renderer, renderingView, *descriptorAccessAdapter, *runtimeStateAdapter, compositionAdapter.get(), *camera, window));

        renderPipeline->setRenderGraph(std::move(graph));
    }

    void App::run() {
        auto currentTime   = std::chrono::high_resolution_clock::now();
        auto lastHeartbeat = currentTime;

        while (!window.shouldClose()) {
            glfwPollEvents();

            // F11: toggle exclusive fullscreen (edge-detect)
            static bool f11WasDown = false;
            int         f11State   = glfwGetKey(window.getGLFWwindow(), GLFW_KEY_F3);
            if (f11State == GLFW_PRESS && !f11WasDown) {
                window.toggleFullscreen();
                f11WasDown = true;
            } else if (f11State == GLFW_RELEASE) {
                f11WasDown = false;
            }

            auto  newTime   = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
            currentTime     = newTime;
            frameTime       = glm::min(frameTime, 0.1f);

            update(frameTime);
            render(frameTime);

            // Heartbeat log to detect freezes (once per second)
            if (std::chrono::duration<float>(newTime - lastHeartbeat).count() >= 1.0f) {
                lastHeartbeat = newTime;
            }
        }

        device.WaitIdle();
    }

    void App::update(float frameTime) {
        auto resources = engineState.resourceService().view();
        if (resources.resourceManager != nullptr) {
            resources.resourceManager->updateAsyncCallbacks();
        }

        auto runtimeState = sceneRuntimeState();

        if (reconcileSceneLoadUseCase != nullptr) {
            reconcileSceneLoadUseCase->execute(runtimeState);
        }

        if (processSceneSelectionMaintenanceUseCase != nullptr) {
            processSceneSelectionMaintenanceUseCase->execute(runtimeState);
        }

        if (syncEnvironmentLightingUseCase != nullptr) {
            auto       rendering         = engineState.renderingService().view();
            bool const showSkyboxEnabled = (rendering.showSkybox != nullptr) && *rendering.showSkybox;
            syncEnvironmentLightingUseCase->execute(showSkyboxEnabled);
        }
    }

    SceneRuntimeState App::sceneRuntimeState() {
        auto sceneRuntime = engineState.sceneRuntimeService().view();
        return SceneRuntimeState{
            .physicsSimulationRunning          = *sceneRuntimeAccessAdapter->physicsSimulationRunning(),
            .selectedEntity                    = *sceneRuntime.selectedEntity,
            .cameraEntity                      = *sceneRuntime.cameraEntity,
            .selectedObjectId                  = selectedObjectId,
            .pendingUpdateCameraAfterSceneLoad = pendingUpdateCameraAfterSceneLoad,
            .solidGroundEnabled                = *sceneRuntimeAccessAdapter->solidGroundEnabled(),
        };
    }

    void App::render(float frameTime) {
        if (auto commandBuffer = renderer.beginFrame()) {
            if (renderer.wasSwapChainRecreated()) {
                // PostProcessingSystem lives in EngineState — recreate via port adapter.
                postProcessingAccessAdapter->recreatePostProcessingSystemWithExistingLayout(device, renderer.getSwapChainRenderPass());
            }

            int const frameIndex      = renderer.getFrameIndex();
            auto      renderingState  = engineState.renderingService().view();
            auto      sceneRuntime    = engineState.sceneRuntimeService().view();
            auto*     animationSystem = animationAccessAdapter->getAnimationSystem();

            FrameInfo frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = *camera,
                .globalDescriptorSet = renderingState.renderContextPort->getGlobalDescriptorSet(frameIndex),
                .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
                .scene               = sceneRuntime.scene,
                .selectedObjectId    = selectedObjectId,
                .selectedEntity      = *sceneRuntime.selectedEntity,
                .cameraEntity        = *sceneRuntime.cameraEntity,
                .morphManager        = animationSystem ? animationSystem->getMorphManager() : nullptr,
                .extent              = renderer.getSwapChainExtent(),
                .debugMode           = debugMode,
            };

            renderPipeline->execute(frameInfo);

            selectedObjectId             = frameInfo.selectedObjectId;
            *sceneRuntime.selectedEntity = frameInfo.selectedEntity;
            *sceneRuntime.cameraEntity   = frameInfo.cameraEntity;

            renderer.endFrame();
        }
    }

}  // namespace engine
