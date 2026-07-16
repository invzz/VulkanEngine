#include "Editor/app.hpp"

#include <glm/common.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <cstring>
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
#include "Engine/Profiling/FrameProfiler.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
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
#include "Engine/Systems/SelectionCompositeSystem.hpp"
#include "Engine/Systems/SelectionMaskSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "ModelLib/Resources/TextureManager.hpp"
#include "vulkan/vulkan_core.h"
namespace {
    auto const _vtex_link = &engine::ibl_detail::vtex::loadImage;
}
#include "Engine/Graphics/AccelBuilder.hpp"
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
#include "Editor/SceneLoader.hpp"
#include "Editor/ui/Panels/AtmosphericPanel.hpp"
#include "Editor/ui/Panels/InspectorPanel.hpp"
#include "Editor/ui/Panels/PhysicsPanel.hpp"
#include "Editor/ui/Panels/ProfilerPanel.hpp"
#include "Editor/ui/Panels/ScenePanel.hpp"
#include "Editor/ui/Panels/SettingsPanel.hpp"
#include "Editor/ui/Panels/ToolbarPanel.hpp"
#include "Editor/ui/Panels/ViewportPanel.hpp"
#include "Editor/ui/UIManager.hpp"

namespace engine {

    namespace {

        // Path is shared by startup load, and the UI's Save/Load callbacks —
        // previously the literal "scene.json" was typed three separate times.
        constexpr const char* kSceneFilePath = "scene.json";

        // ---------------------------------------------------------------
        // Pure TLAS-instance-building logic, pulled out of App::render().
        // Templated on the component/container types rather than naming
        // them explicitly, since this file doesn't declare those types
        // itself (they come from headers owned elsewhere) — the template
        // just forwards whatever App passes in.
        // ---------------------------------------------------------------

        template <typename SubMeshT, typename MaterialListT>
        float computeSubmeshOpacity(const SubMeshT& sm, const MaterialListT& materials) {
            float opacity = 1.0f;
            if (sm.materialId >= 0 && sm.materialId < static_cast<int>(materials.size())) {
                const auto& mat = materials[sm.materialId].pbrMaterial;
                if (mat.transmission > 0.001f) {
                    opacity = 1.0f - mat.transmission;
                }
                if (mat.alphaMode == AlphaMode::Blend) {
                    // For alpha-blended materials, the per-texel opacity comes
                    // from the albedo texture alpha channel, not the factor.
                    // When the factor is at default (1.0), we use 0.5 as a
                    // conservative average — the alternative would require
                    // sampling the texture in the shader.
                    float blendOpacity = mat.albedo.a;
                    if (mat.hasAlbedoMap() && mat.albedo.a >= 0.999f) {
                        blendOpacity = 0.5f;
                    }
                    opacity = std::min(opacity, blendOpacity);
                }
            }
            return opacity;
        }

        /// Appends one model's BLAS instance and per-submesh opacity data to the
        /// TLAS build buffers. Returns false (and appends nothing) if the entity
        /// has no model or the model has no built BLAS yet.
        ///
        /// Submeshes are stored in primitive order (the TLAS primitiveId maps to
        /// the submesh index within the model).
        template <typename ModelComponentT, typename TransformComponentT, typename TlasInstanceVecT>
        bool appendTlasInstance(
            const ModelComponentT&     mc,
            const TransformComponentT& tc,
            AccelBuilder&              accelBuilder,
            TlasInstanceVecT&          tlasInstances,
            std::vector<uint32_t>&     submeshHeaders,
            std::vector<uint32_t>&     submeshData) {
            if (!mc.model) {
                return false;
            }
            const VkAccelerationStructureKHR blas = accelBuilder.getBlas(*mc.model);
            if (blas == VK_NULL_HANDLE) {
                return false;
            }
            tlasInstances.emplace_back(tc.modelTransform(), blas);

            const auto& subMeshes = mc.model->getSubMeshes();
            const auto& materials = mc.model->getMaterials();

            bool allOpaque = true;
            for (const auto& sm : subMeshes) {
                const float opacity = computeSubmeshOpacity(sm, materials);
                if (opacity < 0.999f) {
                    allOpaque = false;
                    break;
                }
            }

            uint32_t hdrCount = static_cast<uint32_t>(subMeshes.size());
            if (allOpaque) {
                hdrCount |= 0x80000000u;  // High bit = all submeshes opaque
            }
            submeshHeaders.push_back(static_cast<uint32_t>(submeshData.size() / 3));  // offset in struct entries
            submeshHeaders.push_back(hdrCount);                                       // count with allOpaque flag

            for (const auto& sm : subMeshes) {
                const float    opacity  = computeSubmeshOpacity(sm, materials);
                const uint32_t startTri = sm.indexOffset / 3;
                const uint32_t endTri   = (sm.indexOffset + sm.indexCount) / 3;
                uint32_t       opacityBits;
                std::memcpy(&opacityBits, &opacity, sizeof(float));
                submeshData.push_back(startTri);
                submeshData.push_back(endTri);
                submeshData.push_back(opacityBits);
            }
            return true;
        }

        RuntimeSettingsBindings makeRuntimeSettingsBindings(
            EngineState& engineState,
            bool&        multithreadedRecordingEnabled,
            uint32_t&    multithreadedRecordingThreads,
            int&         debugMode) {
            return RuntimeSettingsBindings{
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
            };
        }

        void loadStartupSceneIfPresent(EngineState& engineState, ResourceManager& resourceManager, Device& device, const std::string& modelPath) {
            Scene& scene = engineState.scene();
            // If a model file was specified on the command line, load it directly.
            if (!modelPath.empty()) {
                engine::Logger::info(engine::LogChannel::General, "[App] Loading model from command line: ", modelPath);
                SceneLoader::createFromFile(device, scene, resourceManager, modelPath);
                return;
            }
            if (!std::filesystem::exists(kSceneFilePath)) {
                return;
            }
            engine::Logger::info(engine::LogChannel::General, "[App] Loading scene.json at startup...");
            if (engineState.loadScene(kSceneFilePath)) {
                engine::Logger::info(engine::LogChannel::General, "[App] Loaded scene.json");
            }
        }

    }  // namespace

    App::App(bool fullscreen, std::string modelPath)
        : window(width(), height(), "Vulkan Editor", fullscreen),
          device(window),
          renderer(window, device),
          resourceManager(device),
          sceneSerializer(engineState.scene(), resourceManager),
          modelPath_(std::move(modelPath)) {
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
        GpuProfiler::instance().exportLastFrameCsv("gpu_profile.csv");
        GpuProfiler::instance().shutdown();
    }

    void App::init() {
        device.enableThreadLocalCommandPools();
        renderContext        = std::make_unique<RenderContext>(device, resourceManager.getMeshManager());
        renderContextAdapter = std::make_unique<RenderContextAdapter>(renderContext.get());
        // Create AccelBuilder if raytracing is supported
        if (device.rayQuerySupported()) {
            accelBuilder = std::make_unique<AccelBuilder>(device);
            // ModelLib builds the BLAS at load time via this injected callback,
            // keeping the loading layer decoupled from Engine's raytracing layer.
            resourceManager.setModelLoadedCallback([this](Model& m) { accelBuilder->buildBlas(m); });
            renderContext->setAccelBuilder(accelBuilder.get());
        }
        // Load model/scene before setupScene so the scene is empty when
        // loading a command-line model (createFromFile skips if non-empty).
        loadStartupSceneIfPresent(engineState, resourceManager, device, modelPath_);
        setupScene();
        engineState.initialize(device, renderer, resourceManager,
            renderContextAdapter.get(), &window,
            multithreadedRecordingEnabled, multithreadedRecordingThreads);
        engineState.setSerializer(&sceneSerializer);
        sceneSerializer.setRuntimeSettingsBindings(makeRuntimeSettingsBindings(
            engineState, multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode));
        setupUI();
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
        GpuProfiler::instance().setEnabled(true);
    }

    void App::setupScene() {
        camera          = std::make_unique<Camera>();
        auto  camEntity = engineState.createEntity();
        auto& registry  = engineState.scene().getRegistry();
        registry.emplace<TransformComponent>(camEntity);
        registry.emplace<NameComponent>(camEntity, "Camera");
        registry.get<TransformComponent>(camEntity).translation = {0.0f, -0.2f, -2.5f};
        registry.emplace<CameraComponent>(camEntity);
        engineState.setCameraEntity(camEntity);
    }

    void App::setupUI() {
        imguiManager = std::make_unique<ImGuiManager>(window, device, renderer.getSwapChainRenderPass(),
            static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
        uiManager    = std::make_unique<UIManager>(*imguiManager);
        uiManager->setOnSaveScene([this]() {
            engineState.saveScene(kSceneFilePath);
        });
        uiManager->setOnLoadScene([this]() {
            engineState.loadScene(kSceneFilePath);
        });
        auto& registry   = uiManager->getPanelRegistry();
        auto  scenePanel = std::make_unique<ScenePanel>(device, engineState);
        registry.registerPanel("Scene Objects", std::move(scenePanel), DockConstraints{.preferredZone = DockZone::DockLeft, .minSizeX = 250.0f, .minSizeY = 200.0f});
        auto inspectorPanel = std::make_unique<InspectorPanel>(engineState);
        registry.registerPanel("Inspector", std::move(inspectorPanel), DockConstraints{.preferredZone = DockZone::DockRight, .minSizeX = 300.0f, .minSizeY = 200.0f});
        auto settingsPanel = std::make_unique<SettingsPanel>(&engineState,
            multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode,
            rtDirectional, rtPoint, rtSpot, rtShadowSoftness);
        registry.registerPanel("Settings", std::move(settingsPanel), DockConstraints{.preferredZone = DockZone::None, .dockable = false, .floatable = true, .minSizeX = 420.0f, .minSizeY = 260.0f});
        registry.hidePanel("Settings");
        auto physicsPanel = std::make_unique<PhysicsPanel>(engineState);
        registry.registerPanel("Physics", std::move(physicsPanel), DockConstraints{.preferredZone = DockZone::DockCenter, .minSizeX = 300.0f, .minSizeY = 200.0f});
        auto atmosphericPanel = std::make_unique<AtmosphericPanel>(engineState.skySettings());
        registry.registerPanel("Atmospheric", std::move(atmosphericPanel), DockConstraints{.preferredZone = DockZone::None, .dockable = false, .floatable = true, .minSizeX = 320.0f, .minSizeY = 300.0f});
        registry.hidePanel("Atmospheric");
        auto toolbar = std::make_unique<ToolbarPanel>();
        toolbar->setSettingsPanel(registry.getPanel("Settings"));
        toolbar->addToggle("Scene", registry.getPanel("Scene Objects"));
        toolbar->addToggle("Inspector", registry.getPanel("Inspector"));
        toolbar->addToggle("Physics", registry.getPanel("Physics"));
        toolbar->addToggle("Atmospheric", registry.getPanel("Atmospheric"));
        uiManager->setToolbarPanel(std::move(toolbar));
        uiManager->addToolbarToggle("Scene", registry.getPanel("Scene Objects"));
        uiManager->addToolbarToggle("Inspector", registry.getPanel("Inspector"));
        uiManager->addToolbarToggle("Physics", registry.getPanel("Physics"));
        uiManager->addToolbarToggle("Atmospheric", registry.getPanel("Atmospheric"));
        auto vp        = std::make_unique<ViewportPanel>();
        viewportPanel_ = vp.get();
        registry.registerPanel("Viewport", std::move(vp), DockConstraints{.preferredZone = DockZone::DockCenter, .minSizeX = 400.0f, .minSizeY = 300.0f});

        auto profilerPanel = std::make_unique<ProfilerPanel>();
        registry.registerPanel("Profiler", std::move(profilerPanel), DockConstraints{.preferredZone = DockZone::DockBottom, .minSizeX = 400.0f, .minSizeY = 200.0f});
        registry.hidePanel("Profiler");
        uiManager->addToolbarToggle("Profiler", registry.getPanel("Profiler"));
        // Also add directly to ToolbarPanel via WorkspaceManager so it renders.
        if (auto* tb = uiManager->getWorkspaceManager().getToolbarPanel()) {
            tb->addToggle("Profiler", registry.getPanel("Profiler"));
        }
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
            engineState.shadowSettings(),
            rtShadowSoftness));
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
            *engineState.systemPtr<SkyboxRenderSystem>(),
            renderer,
            engineState.editor(),
            engineState.skybox(),
            engineState.skySettings()));
        // Selection mask: render selected geometry (depth disabled) into a 1-channel
        // mask so the full silhouette is captured. Runs after the scene, before the
        // screen-space composite that turns it into the Blender-style rim.
        graph->addPass(std::make_unique<LambdaRenderPass>("SelectionMask",
            [this](FrameInfo& frameInfo) {
                renderer.beginSelectionMaskRenderPass(frameInfo.commandBuffer);
                engineState.system<SelectionMaskSystem>().render(frameInfo);
                renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<LambdaRenderPass>("TransitionToReadOnly",
            [this](FrameInfo& frameInfo) {
                renderer.transitionColorToShaderReadOnly(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<PostProcessPass>(renderer, engineState));
        // Selection composite: full-screen edge-detect on the mask, blended on top of
        // the tonemapped post-fx image. This is the topmost scene layer (above all
        // geometry and post-fx) before the ImGui viewport overlay.
        graph->addPass(std::make_unique<LambdaRenderPass>("SelectionComposite",
            [this](FrameInfo& frameInfo) {
                renderer.beginSelectionOutlineRenderPass(frameInfo.commandBuffer);
                engineState.system<SelectionCompositeSystem>().render(frameInfo);
                renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<CompositionPass>(renderer, [this](FrameInfo& frameInfo, VkCommandBuffer cmd, bool cursorVisible) { uiManager->render(frameInfo, cmd, cursorVisible); }, window));
        renderPipeline->setRenderGraph(std::move(graph));
    }

    void App::run() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto lastFpsTime = currentTime;
        int  frameCount  = 0;
        while (!window.shouldClose()) {
            glfwPollEvents();
            auto  newTime   = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
            currentTime     = newTime;
            frameTime       = glm::min(frameTime, 0.1f);

            auto& profiler = FrameProfiler::instance();
            profiler.beginFrame();

            {
                ScopedCpuSection updateSec("Update");
                update(frameTime);
            }
            {
                ScopedCpuSection renderSec("Render");
                render(frameTime);
            }

            profiler.endFrame();

            frameCount++;
            auto elapsed = std::chrono::duration<float>(newTime - lastFpsTime).count();
            if (elapsed >= 1.0f) {
                float fps = static_cast<float>(frameCount) / elapsed;
                Logger::info(LogChannel::Render, "FPS: ", fps, "  (", frameCount, " frames in ", elapsed, "s)");
                frameCount  = 0;
                lastFpsTime = newTime;
            }
        }
        device.WaitIdle();
    }

    void App::update(float /*frameTime*/) {
        {
            ScopedCpuSection sec("AsyncCallbacks");
            engineState.resourceManager().updateAsyncCallbacks();
        }
        {
            ScopedCpuSection sec("SceneReconcile");
            engineState.reconcileSceneLoad();
        }
        auto showSkybox = &engineState.showSkybox();
        if (showSkybox != nullptr) {
            ScopedCpuSection sec("EnvLightSync");
            engineState.syncEnvironmentLighting(*showSkybox);
        }
        {
            ScopedCpuSection sec("SunLightSync");
            engineState.updateSunLight();
        }
    }

    void App::handleViewportResize() {
        const VkExtent2D panelExtent = (viewportPanel_ != nullptr) ? viewportPanel_->getExtent() : VkExtent2D{0, 0};
        const VkExtent2D fbExtent    = renderer.getOffscreenExtent();
        const bool       wantResize  = viewportResize_.pending_ ||
                                       (panelExtent.width > 0 && panelExtent.height > 0 &&
                                           (panelExtent.width != fbExtent.width || panelExtent.height != fbExtent.height));
        if (!wantResize) {
            return;
        }

        const VkExtent2D targetExtent = (viewportResize_.pending_ && viewportResize_.extent_.width > 0)
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

    FrameInfo App::buildFrameInfo(int frameIndex, float frameTime, VkCommandBuffer commandBuffer) {
        return FrameInfo{
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
            .viewportMousePos       = pendingViewportMousePos_,
            .viewportMouseClicked   = pendingViewportClick_,
            .debugMode              = debugMode,
            .gizmoOperation         = engineState.editor().gizmoOperation,
            .gizmoMode              = engineState.editor().gizmoMode,
            .gizmoEnabled           = engineState.editor().gizmoEnabled,
            .viewGizmoOrbitSelected = engineState.editor().viewGizmoOrbitSelected,
            .rtDirectional          = rtDirectional,
            .rtPoint                = rtPoint,
            .rtSpot                 = rtSpot,
        };
    }

    void App::applyViewportPicking(FrameInfo& frameInfo) {
        // Consume the pending click seeded from the previous frame's UI capture.
        pendingViewportClick_ = false;
        if (!frameInfo.viewportMouseClicked) {
            return;
        }
        PickingSystem pickingSystem;
        pickingSystem.setSpatialSystem(&engineState.spatialSystem());
        auto pickResult = pickingSystem.pickViewport(
            frameInfo, frameInfo.viewportMousePos.x, frameInfo.viewportMousePos.y);
        if (pickResult.has_value()) {
            frameInfo.selectedEntity   = pickResult.value();
            frameInfo.selectedObjectId = static_cast<uint32_t>(pickResult.value());
        } else {
            frameInfo.selectedEntity   = entt::null;
            frameInfo.selectedObjectId = 0;
        }
    }

    void App::rebuildAccelerationStructures(VkCommandBuffer commandBuffer) {
        if (!accelBuilder) {
            return;
        }

        auto view = engineState.scene().getRegistry().view<ModelComponent, TransformComponent>();
        if (view.size_hint() == 0) {
            return;  // No models in scene, nothing to rebuild.
        }

        tlasInstances_.clear();
        instanceSubmeshHeaders_.clear();
        instanceSubmeshData_.clear();

        for (auto entity : view) {
            auto [mc, tc] = view.get<ModelComponent, TransformComponent>(entity);
            appendTlasInstance(mc, tc, *accelBuilder, tlasInstances_, instanceSubmeshHeaders_, instanceSubmeshData_);
        }

        // Skip TLAS rebuild when there are no instances — avoids 0-size buffer/VkBuffer
        // creation that violates Vulkan spec and crashes on RADV and other drivers.
        if (!tlasInstances_.empty()) {
            renderContext->rebuildTlas(tlasInstances_, instanceSubmeshHeaders_, instanceSubmeshData_, commandBuffer);
        }
    }

    void App::syncStateFromFrame(const FrameInfo& frameInfo) {
        engineState.editor().viewportSettings.mode = frameInfo.viewportMode;
        selectedObjectId                           = frameInfo.selectedObjectId;
        // The UI (which runs last in the pipeline) may have captured a viewport
        // click into frameInfo this frame. Carry it to the next frame so the
        // picking pass (which runs first) can consume it.
        pendingViewportClick_    = frameInfo.viewportMouseClicked;
        pendingViewportMousePos_ = frameInfo.viewportMousePos;
        engineState.setSelectedEntity(frameInfo.selectedEntity);
        engineState.setCameraEntity(frameInfo.cameraEntity);
        engineState.editor().gizmoOperation         = frameInfo.gizmoOperation;
        engineState.editor().gizmoMode              = frameInfo.gizmoMode;
        engineState.editor().gizmoEnabled           = frameInfo.gizmoEnabled;
        engineState.editor().viewGizmoOrbitSelected = frameInfo.viewGizmoOrbitSelected;
    }

    void App::render(float frameTime) {
        {
            ScopedCpuSection vpResize("ViewportResize");
            handleViewportResize();
        }

        if (auto commandBuffer = renderer.beginFrame()) {
            if (renderer.wasSwapChainRecreated()) {
                engineState.recreatePostProcessingSystem(device, renderer.getPostFxRenderPass());
            }

            const int frameIndex = renderer.getFrameIndex();
            FrameInfo frameInfo  = buildFrameInfo(frameIndex, frameTime, commandBuffer);

            applyViewportPicking(frameInfo);

            // Rebuild TLAS before the render pipeline executes (deferred pass reads it)
            {
                ScopedCpuSection tlas("RebuildTLAS");
                rebuildAccelerationStructures(commandBuffer);
            }

            // Update the mesh buffer descriptor every frame so newly loaded models
            // are visible to the shader. Only touch the current frame's descriptor
            // to avoid modifying descriptors in-flight on the GPU.
            renderContext->updateMeshDescriptorSet(frameIndex);
            renderPipeline->execute(frameInfo);

            if (auto* scenePanel = uiManager->getPanel<ScenePanel>()) {
                scenePanel->processDelayedDeletions(frameInfo.selectedEntity, frameInfo.selectedObjectId);
            }

            syncStateFromFrame(frameInfo);
            renderer.endFrame();
        }
    }

}  // namespace engine