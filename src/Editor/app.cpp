#include "Editor/app.hpp"

#include <glm/common.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Graphics/Viewport.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "ModelLib/Resources/TextureManager.hpp"
#include "vulkan/vulkan_core.h"

// Systems
#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PickingSystem.hpp"
#include "Engine/Systems/SelectionOutlineSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

// Force VTexIO linkage from ModelLib/Texture.cpp symbols.
#include "Engine/Systems/IBL/VTexIO.hpp"
namespace {
    auto const _vtex_link = &engine::ibl_detail::vtex::loadImage;
}

// Render Passes
#include "Engine/Graphics/Passes/CompositionPass.hpp"
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Graphics/Passes/DeferredLightingPass.hpp"
#include "Engine/Graphics/Passes/DepthPrepass.hpp"
#include "Engine/Graphics/Passes/ForwardPass.hpp"
#include "Engine/Graphics/Passes/GbufferPass.hpp"
#include "Engine/Graphics/Passes/ShadowPass.hpp"
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Editor/RenderContext.hpp"

// UI Panels
#include "Editor/ui/Panels/InspectorPanel.hpp"
#include "Editor/ui/Panels/PhysicsPanel.hpp"
#include "Editor/ui/Panels/Scene/ScenePanel.hpp"
#include "Editor/ui/Panels/SettingsPanel.hpp"
#include "Editor/ui/Panels/ToolbarPanel.hpp"
#include "Editor/ui/Panels/ViewportPanel.hpp"
#include "Editor/ui/UIManager.hpp"

namespace engine {

    App::App(bool fullscreen)
        : window(width(), height(), "Vulkan Editor", fullscreen),
          device(window),
          renderer(window, device),
          resourceManager(device),
          sceneSerializer(engineState.scene(), resourceManager) {
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
            std::cerr << "[App::~App] Shutdown drain failed\n";
        }
        GpuProfiler::instance().shutdown();
    }

    void App::init() {
        device.enableThreadLocalCommandPools();

        renderContext        = std::make_unique<RenderContext>(device, resourceManager.getMeshManager());
        renderContextAdapter = std::make_unique<RenderContextAdapter>(renderContext.get());

        setupScene();

        engineState.initialize(device, renderer, resourceManager,
            renderContextAdapter.get(), &window,
            multithreadedRecordingEnabled, multithreadedRecordingThreads);
        engineState.setSerializer(&sceneSerializer);

        // Sceneserializer bindings
        sceneSerializer.setRuntimeSettingsBindings(RuntimeSettingsBindings{
            .showSkybox                    = &engineState.showSkybox(),
            .showGrid                      = &engineState.showGrid(),
            .showDebugObjects              = &engineState.showDebugObjects(),
            .physicsSimulationRunning      = &engineState.physicsRunning(),
            .skySettings                   = &engineState.skySettings(),
            .postProcessPush               = &engineState.postProcess(),
            .iblSystem                     = &engineState.system<IBLSystem>(),
            .modelRenderSystem             = &engineState.system<ModelRenderSystem>(),
            .getGpuProfilerEnabled         = []() { return GpuProfiler::instance().isEnabled(); },
            .setGpuProfilerEnabled         = [](bool enabled) { GpuProfiler::instance().setEnabled(enabled); },
            .multithreadedRecordingEnabled = &multithreadedRecordingEnabled,
            .multithreadedRecordingThreads = &multithreadedRecordingThreads,
            .debugMode                     = &debugMode,
        });

        setupUI();

        if (std::filesystem::exists("scene.json")) {
            std::cout << "[App] Loading scene.json at startup...\n";
            if (engineState.loadScene("scene.json")) {
                std::cout << "[App] Loaded scene.json\n";
            }
        }

        renderPipeline = std::make_unique<RenderPipeline>(renderer);
        setupRenderGraph();

        viewport_.create(device, renderer);
        if (viewportPanel_) {
            viewportPanel_->setViewport(&viewport_, renderer.getSwapChainExtent());
            viewportPanel_->setWindow(&window);
            viewportPanel_->setMouse(engineState.getMouse());
            viewportPanel_->onResize = [this](VkExtent2D extent) {
                // Defer resize to next frame start — we're inside ImGui
                // rendering (mid command buffer) and can't recreate images.
                viewportResize_.pending_ = true;
                viewportResize_.extent_  = extent;
            };
        }

        GpuProfiler::instance().initialize(device.device(),
            device.getProperties().limits.timestampPeriod,
            static_cast<uint32_t>(SwapChain::maxFramesInFlight()), 32);
        GpuProfiler::instance().setEnabled(false);
    }

    void App::setupScene() {
        camera         = std::make_unique<Camera>();
        auto camEntity = engineState.createEntity();
        engineState.scene().getRegistry().emplace<TransformComponent>(camEntity);
        engineState.scene().getRegistry().emplace<NameComponent>(camEntity, "Camera");
        engineState.scene().getRegistry().get<TransformComponent>(camEntity).translation = {0.0f, -0.2f, -2.5f};
        engineState.scene().getRegistry().emplace<CameraComponent>(camEntity);
        engineState.setCameraEntity(camEntity);
    }

    void App::setupUI() {
        imguiManager = std::make_unique<ImGuiManager>(window, device, renderer.getSwapChainRenderPass(),
            static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
        uiManager    = std::make_unique<UIManager>(*imguiManager);

        uiManager->setOnSaveScene([this]() {
            engineState.saveScene("scene.json");
        });
        uiManager->setOnLoadScene([this]() {
            engineState.loadScene("scene.json");
        });

        auto& registry = uiManager->getPanelRegistry();

        auto scenePanel = std::make_unique<ScenePanel>(device, engineState);
        // Registry key MUST match the panel's ImGui::Begin("...") title so
        // DockBuilderDockWindow actually finds the window.
        registry.registerPanel("Scene Objects", std::move(scenePanel), DockConstraints{.preferredZone = DockZone::DockLeft, .minSizeX = 250.0f, .minSizeY = 200.0f});

        auto inspectorPanel = std::make_unique<InspectorPanel>(engineState);
        registry.registerPanel("Inspector", std::move(inspectorPanel), DockConstraints{.preferredZone = DockZone::DockRight, .minSizeX = 300.0f, .minSizeY = 200.0f});

        auto settingsPanel = std::make_unique<SettingsPanel>(&engineState,
            multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode);
        registry.registerPanel("Settings", std::move(settingsPanel), DockConstraints{.preferredZone = DockZone::None, .dockable = false, .floatable = true, .minSizeX = 420.0f, .minSizeY = 260.0f});
        registry.hidePanel("Settings");

        auto physicsPanel = std::make_unique<PhysicsPanel>(engineState);
        registry.registerPanel("Physics", std::move(physicsPanel), DockConstraints{.preferredZone = DockZone::DockCenter, .minSizeX = 300.0f, .minSizeY = 200.0f});

        auto toolbar = std::make_unique<ToolbarPanel>();
        toolbar->setSettingsPanel(registry.getPanel("Settings"));
        uiManager->setToolbarPanel(std::move(toolbar));
        uiManager->addToolbarToggle("Scene", registry.getPanel("Scene Objects"));
        uiManager->addToolbarToggle("Inspector", registry.getPanel("Inspector"));
        uiManager->addToolbarToggle("Physics", registry.getPanel("Physics"));

        auto vp        = std::make_unique<ViewportPanel>();
        viewportPanel_ = vp.get();
        registry.registerPanel("Viewport", std::move(vp), DockConstraints{.preferredZone = DockZone::DockCenter, .minSizeX = 400.0f, .minSizeY = 300.0f});
    }

    void App::setupRenderGraph() {
        auto graph = std::make_unique<RenderGraph>();

        // UpdatePass
        graph->addPass(std::make_unique<UpdatePass>(
            engineState.systemPtr<ObjectSelectionSystem>(),
            engineState.systemPtr<InputSystem>(),
            engineState.systemPtr<JoltPhysicsSystem>(),
            engineState.physicsRunning(),
            renderer));

        // ComputePass
        graph->addPass(std::make_unique<ComputePass>(engineState.systemPtr<AnimationSystem>()));

        // ShadowPass
        graph->addPass(std::make_unique<ShadowPass>(
            engineState.system<ShadowSystem>(),
            engineState.renderContext(),
            engineState.scene(),
            engineState.shadowSettings()));

        // DepthPrepass
        graph->addPass(std::make_unique<DepthPrepass>(
            engineState.system<ModelRenderSystem>(),
            renderer));

        // GbufferPass (systems via DI, descriptors via EngineState per-frame)
        graph->addPass(std::make_unique<GbufferPass>(
            engineState.system<ModelRenderSystem>(),
            engineState, renderer,
            engineState.renderContext()));

        // DeferredLightingPass
        graph->addPass(std::make_unique<DeferredLightingPass>(
            engineState.system<DeferredLightingSystem>(),
            engineState.system<ShadowSystem>(),
            engineState, renderer, device,
            engineState.renderContext()));

        // ForwardPass (transparency + debug + mipmaps)
        graph->addPass(std::make_unique<ForwardPass>(
            engineState.system<ModelRenderSystem>(),
            engineState.system<GridRenderSystem>(),
            engineState.system<LightSystem>(),
            engineState.system<CameraSystem>(),
            engineState.system<ColliderDebugRenderSystem>(),
            engineState.system<SelectionOutlineSystem>(),
            renderer,
            engineState.editor()));

        // TransitionColorToReadOnly — transition offscreen color from
        // COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
        // so ImGui can sample it directly (no copy needed).
        graph->addPass(std::make_unique<LambdaRenderPass>("TransitionToReadOnly",
            [this](FrameInfo& frameInfo) {
                renderer.transitionColorToShaderReadOnly(frameInfo.commandBuffer);
            }));

        // CompositionPass — UI overlay via callback instead of port interface
        graph->addPass(std::make_unique<CompositionPass>(renderer, [this](FrameInfo& frameInfo, VkCommandBuffer cmd, bool cursorVisible) { uiManager->render(frameInfo, cmd, cursorVisible); }, window));

        renderPipeline->setRenderGraph(std::move(graph));
    }

    void App::run() {
        auto currentTime = std::chrono::high_resolution_clock::now();

        while (!window.shouldClose()) {
            glfwPollEvents();

            auto  newTime   = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
            currentTime     = newTime;
            frameTime       = glm::min(frameTime, 0.1f);

            update(frameTime);
            render(frameTime);
        }

        device.WaitIdle();
    }

    void App::update(float frameTime) {
        engineState.resourceManager().updateAsyncCallbacks();

        engineState.reconcileSceneLoad();

        auto showSkybox = &engineState.showSkybox();
        if (showSkybox != nullptr) {
            engineState.syncEnvironmentLighting(*showSkybox);
        }
    }

    void App::render(float frameTime) {
        // Pre-beginFrame resize: if the panel's current extent differs from
        // the offscreen FB extent, resize the FB now (before we touch any
        // command buffer). This catches:
        //   1. The previous frame's onResize() (pending_ flag).
        //   2. The first-frame case where the FB was created at swapchain
        //      extent and the panel is smaller.
        //   3. Any case where the panel's getExtent() drifted out of sync
        //      with the FB (defensive).
        //
        // IMPORTANT: the FB MUST be resized BEFORE the render graph runs, not
        // after. The render graph records commands that reference the FB's
        // images, so destroying the FB mid-frame would create a use-after-free
        // (the command buffer would hold a stale VkImageView, and the GPU
        // would fault on submission with a GPUVM permission error).
        {
            VkExtent2D panelExtent = viewportPanel_ ? viewportPanel_->getExtent() : VkExtent2D{0, 0};
            VkExtent2D fbExtent    = renderer.getOffscreenExtent();
            bool       wantResize  = viewportResize_.pending_ ||
                                     (panelExtent.width > 0 && panelExtent.height > 0 &&
                                         (panelExtent.width != fbExtent.width || panelExtent.height != fbExtent.height));
            if (wantResize) {
                VkExtent2D targetExtent = (viewportResize_.pending_ && viewportResize_.extent_.width > 0)
                                              ? viewportResize_.extent_
                                              : panelExtent;
                vkDeviceWaitIdle(device.device());
                viewport_.resize(device, renderer, targetExtent);
                viewportResize_.pending_ = false;
                if (viewportPanel_) {
                    viewportPanel_->setViewport(&viewport_, targetExtent);
                }
            }
        }

        if (auto commandBuffer = renderer.beginFrame()) {
            if (renderer.wasSwapChainRecreated()) {
                engineState.recreatePostProcessingSystem(device, renderer.getSwapChainRenderPass());
                // Do NOT recreate the offscreen FB here. The cached
                // offscreenExtent_ is the OLD panel size (the panel hasn't
                // been re-laid-out yet at beginFrame() time). The panel's
                // render() in the composition pass will report the new size
                // via onResize(), and the pre-beginFrame block at the start
                // of the NEXT frame will resize the FB then.
            }

            int frameIndex = renderer.getFrameIndex();

            PickingSystem pickingSystem;
            FrameInfo     frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = *camera,
                .globalDescriptorSet = engineState.renderContext().getGlobalDescriptorSet(frameIndex),
                .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
                .scene               = &engineState.scene(),
                .selectedObjectId    = selectedObjectId,
                .selectedEntity      = engineState.selectedEntity(),
                .cameraEntity        = engineState.cameraEntity(),
                .morphManager        = engineState.systemPtr<AnimationSystem>()
                                           ? engineState.system<AnimationSystem>().getMorphManager()
                                           : nullptr,
                .extent              = renderer.getOffscreenExtent(),
                .viewportMode        = engineState.editor().viewportSettings.mode,
                .debugMode           = debugMode,
                .gizmoOperation      = engineState.editor().gizmoOperation,
                .gizmoMode           = engineState.editor().gizmoMode,
                .gizmoEnabled        = engineState.editor().gizmoEnabled,
            };

            // Mouse picking: the ViewportPanel has already converted the click
            // to viewport-normalized coordinates; just execute the ray-AABB test.
            if (frameInfo.viewportMouseClicked) {
                auto pickResult = pickingSystem.pickViewport(frameInfo,
                    frameInfo.viewportMousePos.x,
                    frameInfo.viewportMousePos.y);
                if (pickResult.has_value()) {
                    frameInfo.selectedEntity   = pickResult.value();
                    frameInfo.selectedObjectId = static_cast<uint32_t>(pickResult.value());
                } else {
                    frameInfo.selectedEntity   = entt::null;
                    frameInfo.selectedObjectId = 0;
                }
            }

            renderPipeline->execute(frameInfo);

            // Persist mode changes made by the viewport panel back to EditorState.
            engineState.editor().viewportSettings.mode = frameInfo.viewportMode;
            selectedObjectId                           = frameInfo.selectedObjectId;
            engineState.setSelectedEntity(frameInfo.selectedEntity);
            engineState.setCameraEntity(frameInfo.cameraEntity);
            engineState.editor().gizmoOperation = frameInfo.gizmoOperation;
            engineState.editor().gizmoMode      = frameInfo.gizmoMode;
            engineState.editor().gizmoEnabled   = frameInfo.gizmoEnabled;

            renderer.endFrame();
        }
    }

}  // namespace engine
