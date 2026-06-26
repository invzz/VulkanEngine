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
#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/PickingSystem.hpp"
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

        // 6. Initialize viewport texture (after EngineState and render graph)
        viewportTexture_.create(device, VkExtent2D{1024, 768});

        // Initialize viewport display (renders HDR viewport texture to swap chain)
        viewportDisplay_.initialize(
            device,
            renderer.getSwapChainRenderPass(),
            renderer.getSwapChainFormat(),
            renderer.getSwapChainExtent());

        // Initialize viewport panel (must come after viewportTexture_.create()
        // because the texture's image view is needed for ImGui registration)
        if (viewportPanel_) {
            viewportPanel_->initialize(*imguiManager, &viewportTexture_, VkExtent2D{600, 400});
        }

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

        // --- Viewport panel ---
        viewportPanel_ = std::make_unique<ViewportPanel>();
        registry.registerPanel("Viewport", std::move(viewportPanel_), DockConstraints{
                                                                          .preferredZone = DockZone::DockCenter,
                                                                          .minSizeX      = 400.0f,
                                                                          .minSizeY      = 300.0f,
                                                                      });
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

        // 5.5. Copy offscreen color to viewport texture + render to swap chain
        graph->addPass(std::make_unique<LambdaRenderPass>("ViewportCopy",
            [this](FrameInfo& frameInfo) {
                // Copy offscreen color to viewport texture
                VkImage srcImage = renderer.getOffscreenColorImage(frameInfo.frameIndex);

                VkImageMemoryBarrier srcBarrier{};
                srcBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                srcBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                srcBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                srcBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                srcBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                srcBarrier.image                           = srcImage;
                srcBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                srcBarrier.subresourceRange.baseMipLevel   = 0;
                srcBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
                srcBarrier.subresourceRange.baseArrayLayer = 0;
                srcBarrier.subresourceRange.layerCount     = 1;
                srcBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                srcBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(frameInfo.commandBuffer,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &srcBarrier);

                // Transition viewport texture to TRANSFER_DST_OPTIMAL
                VkImageLayout srcLayout = viewportTextureInitialized_
                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_UNDEFINED;
                viewportTexture_.transitionToTransferDst(frameInfo.commandBuffer, srcLayout);
                viewportTextureInitialized_ = true;

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.mipLevel       = 0;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.layerCount     = 1;
                copyRegion.srcOffset                     = {0, 0, 0};
                copyRegion.dstSubresource                = copyRegion.srcSubresource;
                copyRegion.dstOffset                     = {0, 0, 0};

                VkExtent2D const texExtent = viewportTexture_.getExtent();
                VkExtent2D const srcExtent = frameInfo.extent;
                copyRegion.extent.width  = std::min(texExtent.width, srcExtent.width);
                copyRegion.extent.height = std::min(texExtent.height, srcExtent.height);
                copyRegion.extent.depth  = 1;

                vkCmdCopyImage(frameInfo.commandBuffer,
                    srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    viewportTexture_.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion);

                // Transition viewport texture to SHADER_READ_ONLY
                viewportTexture_.transitionToShaderReadOnly(frameInfo.commandBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                // Render viewport texture to swap chain with tone mapping
                if (viewportDisplay_.isValid()) {
                    viewportDisplay_.setViewportTexture(viewportTexture_);
                    viewportDisplay_.execute(frameInfo.commandBuffer,
                        renderer.getSwapChainFramebuffer(frameInfo.frameIndex));
                }
            }));

        // 6. Composition Pass (PostProcess + UI)
        graph->addPass(std::make_unique<CompositionPass>(renderer, renderingView, *descriptorAccessAdapter, *runtimeStateAdapter, compositionAdapter.get(), *camera, window));

        renderPipeline->setRenderGraph(std::move(graph));
    }

    void App::run() {
        auto currentTime   = std::chrono::high_resolution_clock::now();
        auto lastHeartbeat = currentTime;

        bool f11WasDown = false;

        while (!window.shouldClose()) {
            glfwPollEvents();

            // F3: toggle exclusive fullscreen (edge-detect)
            int f11State = glfwGetKey(window.getGLFWwindow(), GLFW_KEY_F3);
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
                // Update ViewportDisplay with the new swap chain render pass handle.
                // Without this, ViewportDisplay uses a destroyed render pass handle
                // (garbage like 0xe000000000e) causing Vulkan validation error.
                if (viewportDisplay_.isValid()) {
                    viewportDisplay_.setRenderPass(renderer.getSwapChainRenderPass());
                }
            }

            int const frameIndex      = renderer.getFrameIndex();
            auto      renderingState  = engineState.renderingService().view();
            auto      sceneRuntime    = engineState.sceneRuntimeService().view();
            auto*     animationSystem = animationAccessAdapter->getAnimationSystem();

            PickingSystem pickingSystem;
            FrameInfo     frameInfo{
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

            // Mouse picking: left click in the viewport
            static bool lastLeftClick = false;
            int         leftClick     = glfwGetMouseButton(window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT);
            if (leftClick == GLFW_PRESS && !lastLeftClick) {
                double mouseX, mouseY;
                glfwGetCursorPos(window.getGLFWwindow(), &mouseX, &mouseY);
                float aspect     = static_cast<float>(renderer.getSwapChainExtent().width) / renderer.getSwapChainExtent().height;
                auto  pickResult = pickingSystem.pick(frameInfo, mouseX / renderer.getSwapChainExtent().width, mouseY / renderer.getSwapChainExtent().height, aspect);
                if (pickResult.has_value()) {
                    frameInfo.selectedEntity   = pickResult.value();
                    frameInfo.selectedObjectId = static_cast<uint32_t>(pickResult.value());
                } else {
                    // Clicked empty space: deselect
                    frameInfo.selectedEntity   = entt::null;
                    frameInfo.selectedObjectId = 0;
                }
            }
            lastLeftClick = (leftClick == GLFW_PRESS);

            renderPipeline->execute(frameInfo);

            selectedObjectId             = frameInfo.selectedObjectId;
            *sceneRuntime.selectedEntity = frameInfo.selectedEntity;
            *sceneRuntime.cameraEntity   = frameInfo.cameraEntity;

            renderer.endFrame();
        }
    }

}  // namespace engine
