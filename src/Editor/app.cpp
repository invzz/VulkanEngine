#include "Editor/app.hpp"

#include <glm/common.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include "Engine/Core/Logger.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Graphics/Viewport.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PickingSystem.hpp"
#include "Engine/Systems/SelectionOutlineSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "ModelLib/Resources/TextureManager.hpp"
#include "vulkan/vulkan_core.h"
namespace {
    auto const _vtex_link = &engine::ibl_detail::vtex::loadImage;
}
#include "Engine/Graphics/Passes/CompositionPass.hpp"
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Graphics/Passes/DeferredLightingPass.hpp"
#include "Engine/Graphics/Passes/DepthPrepass.hpp"
#include "Engine/Graphics/Passes/ForwardPass.hpp"
#include "Engine/Graphics/Passes/GbufferPass.hpp"
#include "Engine/Graphics/Passes/PostProcessPass.hpp"
#include "Engine/Graphics/Passes/ShadowPass.hpp"
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Editor/RenderContext.hpp"
#include "Editor/ui/Panels/InspectorPanel.hpp"
#include "Editor/ui/Panels/PhysicsPanel.hpp"
#include "Editor/ui/Panels/ScenePanel.hpp"
#include "Editor/ui/Panels/SettingsPanel.hpp"
#include "Editor/ui/Panels/ToolbarPanel.hpp"
#include "Editor/ui/Panels/ViewportPanel.hpp"
#include "Editor/ui/UIManager.hpp"
#include "Engine/Graphics/AccelBuilder.hpp"
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
            engine::Logger::error(engine::LogChannel::General, "[App::~App] Shutdown drain failed: ", e.what());
        } catch (...) {
            engine::Logger::error(engine::LogChannel::General, "[App::~App] Shutdown drain failed");
        }
        GpuProfiler::instance().shutdown();
    }
    void App::init() {
        device.enableThreadLocalCommandPools();
        renderContext        = std::make_unique<RenderContext>(device, resourceManager.getMeshManager());
        renderContextAdapter = std::make_unique<RenderContextAdapter>(renderContext.get());
        // Create AccelBuilder if raytracing is supported
        if (device.rayQuerySupported()) {
            accelBuilder = std::make_unique<AccelBuilder>(device);
            resourceManager.setAccelBuilder(accelBuilder.get());
            renderContext->setAccelBuilder(accelBuilder.get());
        }
        setupScene();
        engineState.initialize(device, renderer, resourceManager,
            renderContextAdapter.get(), &window,
            multithreadedRecordingEnabled, multithreadedRecordingThreads);
        engineState.setSerializer(&sceneSerializer);
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
            .viewGizmoOrbitSelected        = &engineState.editor().viewGizmoOrbitSelected,
        });
        setupUI();
        if (std::filesystem::exists("scene.json")) {
            engine::Logger::info(engine::LogChannel::General, "[App] Loading scene.json at startup...");
            if (engineState.loadScene("scene.json")) {
                engine::Logger::info(engine::LogChannel::General, "[App] Loaded scene.json");
            }
        }
        renderPipeline = std::make_unique<RenderPipeline>(renderer);
        setupRenderGraph();
        viewport_.create(device, renderer);
        if (viewportPanel_ != nullptr) {
            viewportPanel_->setViewport(&viewport_, renderer.getSwapChainExtent());
            viewportPanel_->setWindow(&window);
            viewportPanel_->setMouse(engineState.getMouse());
            viewportPanel_->onResize = [this](VkExtent2D extent) {
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
        auto& registry   = uiManager->getPanelRegistry();
        auto  scenePanel = std::make_unique<ScenePanel>(device, engineState);
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
        graph->addPass(std::make_unique<UpdatePass>(
            engineState.systemPtr<ObjectSelectionSystem>(),
            engineState.systemPtr<InputSystem>(),
            engineState.systemPtr<JoltPhysicsSystem>(),
            engineState.physicsRunning(),
            renderer));
        graph->addPass(std::make_unique<ComputePass>(engineState.systemPtr<AnimationSystem>()));
        graph->addPass(std::make_unique<ShadowPass>(
            engineState.system<ShadowSystem>(),
            engineState.renderContext(),
            engineState.scene(),
            engineState.shadowSettings()));
        graph->addPass(std::make_unique<DepthPrepass>(
            engineState.system<ModelRenderSystem>(),
            renderer));
        graph->addPass(std::make_unique<GbufferPass>(
            engineState.system<ModelRenderSystem>(),
            engineState, renderer,
            engineState.renderContext()));
        graph->addPass(std::make_unique<DeferredLightingPass>(
            engineState.system<DeferredLightingSystem>(),
            engineState.system<ShadowSystem>(),
            engineState, renderer, device,
            engineState.renderContext()));
        graph->addPass(std::make_unique<ForwardPass>(
            engineState.system<ModelRenderSystem>(),
            engineState.system<GridRenderSystem>(),
            engineState.system<LightSystem>(),
            engineState.system<CameraSystem>(),
            engineState.system<ColliderDebugRenderSystem>(),
            engineState.system<SelectionOutlineSystem>(),
            *engineState.systemPtr<SkyboxRenderSystem>(),
            renderer,
            engineState.editor(),
            engineState.skybox(),
            engineState.skySettings()));
        graph->addPass(std::make_unique<LambdaRenderPass>("TransitionToReadOnly",
            [this](FrameInfo& frameInfo) {
                renderer.transitionColorToShaderReadOnly(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<PostProcessPass>(renderer, engineState));
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
    void App::update(float /*frameTime*/) {
        engineState.resourceManager().updateAsyncCallbacks();
        engineState.reconcileSceneLoad();
        auto showSkybox = &engineState.showSkybox();
        if (showSkybox != nullptr) {
            engineState.syncEnvironmentLighting(*showSkybox);
        }
    }
    void App::render(float frameTime) {
        {
            VkExtent2D panelExtent = (viewportPanel_ != nullptr) ? viewportPanel_->getExtent() : VkExtent2D{0, 0};
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
                if (viewportPanel_ != nullptr) {
                    viewportPanel_->setViewport(&viewport_, targetExtent);
                }
                // Post-process descriptor sets reference the offscreen color/depth
                // images which were just destroyed and recreated — update them.
                for (int i = 0; i < SwapChain::maxFramesInFlight(); ++i) {
                    engineState.updatePostProcessDescriptors(i, renderer);
                }
            }
        }
        if (auto commandBuffer = renderer.beginFrame()) {
            if (renderer.wasSwapChainRecreated()) {
                engineState.recreatePostProcessingSystem(device, renderer.getPostFxRenderPass());
            }
            int           frameIndex = renderer.getFrameIndex();
            PickingSystem pickingSystem;
            pickingSystem.setSpatialSystem(&engineState.spatialSystem());
            FrameInfo frameInfo{
                .frameIndex             = frameIndex,
                .frameTime              = frameTime,
                .commandBuffer          = commandBuffer,
                .camera                 = *camera,
                .globalDescriptorSet    = engineState.renderContext().getGlobalDescriptorSet(frameIndex),
                .globalTextureSet       = resourceManager.getTextureManager().getDescriptorSet(),
                .scene                  = &engineState.scene(),
                .selectedObjectId       = selectedObjectId,
                .selectedEntity         = engineState.selectedEntity(),
                .cameraEntity           = engineState.cameraEntity(),
                .morphManager           = engineState.systemPtr<AnimationSystem>() != nullptr
                                              ? engineState.system<AnimationSystem>().getMorphManager()
                                              : nullptr,
                .extent                 = renderer.getOffscreenExtent(),
                .viewportMode           = engineState.editor().viewportSettings.mode,
                .debugMode              = debugMode,
                .gizmoOperation         = engineState.editor().gizmoOperation,
                .gizmoMode              = engineState.editor().gizmoMode,
                .gizmoEnabled           = engineState.editor().gizmoEnabled,
                .viewGizmoOrbitSelected = engineState.editor().viewGizmoOrbitSelected,
            };
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
            // Rebuild TLAS before the render pipeline executes (deferred pass reads it)
            if (accelBuilder) {
                tlasInstances_.clear();
                auto view = engineState.scene().getRegistry().view<ModelComponent, TransformComponent>();
                for (auto entity : view) {
                    auto [mc, tc] = view.get<ModelComponent, TransformComponent>(entity);
                    if (mc.model) {
                        VkAccelerationStructureKHR blas = accelBuilder->getBlas(*mc.model);
                        if (blas != VK_NULL_HANDLE) {
                            tlasInstances_.emplace_back(tc.modelTransform(), blas);
                        }
                    }
                }
                // Skip TLAS rebuild when there are no instances — avoids 0-size buffer/VkBuffer
                // creation that violates Vulkan spec and crashes on RADV and other drivers.
                if (!tlasInstances_.empty()) {
                    renderContext->rebuildTlas(tlasInstances_, commandBuffer);
                } else {
                    // Even when no TLAS instances exist, update the mesh buffer descriptor so
                    // new model entries are visible to the shader next frame.
                    for (int i = 0; i < SwapChain::maxFramesInFlight(); ++i) {
                        renderContext->updateMeshDescriptorSet(i);
                    }
                }
            } else {
                // Raytracing not available — still update mesh buffer descriptor per frame
                // so newly loaded models are visible via the global SSBO.
                for (int i = 0; i < SwapChain::maxFramesInFlight(); ++i) {
                    renderContext->updateMeshDescriptorSet(i);
                }
            }
            renderPipeline->execute(frameInfo);

            if (auto* scenePanel = uiManager->getPanel<ScenePanel>()) {
                scenePanel->processDelayedDeletions(frameInfo.selectedEntity, frameInfo.selectedObjectId);
            }
            engineState.editor().viewportSettings.mode = frameInfo.viewportMode;
            selectedObjectId                           = frameInfo.selectedObjectId;
            engineState.setSelectedEntity(frameInfo.selectedEntity);
            engineState.setCameraEntity(frameInfo.cameraEntity);
            engineState.editor().gizmoOperation         = frameInfo.gizmoOperation;
            engineState.editor().gizmoMode              = frameInfo.gizmoMode;
            engineState.editor().gizmoEnabled           = frameInfo.gizmoEnabled;
            engineState.editor().viewGizmoOrbitSelected = frameInfo.viewGizmoOrbitSelected;
            renderer.endFrame();
        }
    }
}  // namespace engine
