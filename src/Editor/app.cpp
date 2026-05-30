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
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
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
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

// Render Passes
#include "Engine/Graphics/Passes/CompositionPass.hpp"
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Graphics/Passes/DepthPrepass.hpp"
#include "Engine/Graphics/Passes/OffscreenPass.hpp"
#include "Engine/Graphics/Passes/ShadowPass.hpp"
#include "Engine/Graphics/Passes/UpdatePass.hpp"

// Demo specific
#include "Editor/RenderContext.hpp"

// UI Panels
#include "Editor/ui/InspectorPanel.hpp"
#include "Editor/ui/ScenePanel.hpp"
#include "Editor/ui/SettingsPanel.hpp"
#include "Editor/ui/UIManager.hpp"

namespace engine {

    App::App(bool fullscreen)
        : window(width(), height(), "Vulkan Editor", fullscreen), device(window), renderer(window, device), resourceManager(device), sceneSerializer(engineState.getScene(), resourceManager) {
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

        // 1. Setup Render Context (moved into EngineState)
        engineState.renderContext = std::make_unique<RenderContext>(device, resourceManager.getMeshManager());

        // 2. Setup Scene & Camera
        setupScene();

        // 3. Initialize centralized EngineState (systems, descriptors, pools, post-process)
        engineState.initialize(device, renderer, resourceManager, &window, multithreadedRecordingEnabled, multithreadedRecordingThreads);

        sceneSerializer.setRuntimeSettingsBindings(RuntimeSettingsBindings{
            .showSkybox = &engineState.showSkybox,
            .showGrid = &engineState.showGrid,
            .showDebugObjects = &engineState.showDebugObjects,
            .physicsSimulationRunning = &engineState.physicsSimulationRunning,
            .skySettings = &engineState.skySettings,
            .postProcessPush = &engineState.postProcessPush,
            .iblSystem = engineState.iblSystem.get(),
            .modelRenderSystem = engineState.modelRenderSystem.get(),
            .getGpuProfilerEnabled = []() { return GpuProfiler::instance().isEnabled(); },
            .setGpuProfilerEnabled = [](bool enabled) { GpuProfiler::instance().setEnabled(enabled); },
            .multithreadedRecordingEnabled = &multithreadedRecordingEnabled,
            .multithreadedRecordingThreads = &multithreadedRecordingThreads,
            .debugMode = &debugMode,
        });

        // 4. Setup UI
        setupUI();

        // If a scene file exists in the working directory, load it at startup
        if (std::filesystem::exists("scene.json")) {
            std::cout << "[App] Found scene.json, loading at startup..." << '\n';
            if (sceneSerializer.deserialize("scene.json")) {
                std::cout << "[App] Loaded scene.json at startup" << '\n';

                // Physics must always start paused; never auto-run after a scene load.
                engineState.physicsSimulationRunning = false;

                // Reset transient selection state to avoid dangling entt entity references
                engineState.selectedEntity = entt::null;
                selectedObjectId           = 0;
                engineState.cameraEntity   = entt::null;

                // Find the first camera entity in the loaded scene
                auto const& registry = engineState.scene.getRegistry();
                auto        view     = registry.view<engine::CameraComponent>();
                for (auto entity : view) {
                    std::cout << "[App] Found camera entity in loaded scene" << '\n';
                    engineState.cameraEntity = entity;
                    break;
                }

                // If there is no camera, create a default one
                if (engineState.cameraEntity == entt::null) {
                    std::cout << "[App] Creating default camera for the scene" << '\n';
                    engineState.cameraEntity = engineState.scene.createEntity();
                    engineState.scene.getRegistry().emplace<TransformComponent>(engineState.cameraEntity);
                    engineState.scene.getRegistry().emplace<NameComponent>(engineState.cameraEntity, "Camera");
                    engineState.scene.getRegistry().emplace<CameraComponent>(engineState.cameraEntity);
                }

                pendingUpdateCameraAfterSceneLoad = true;
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
        auto sceneState          = engineState.sceneState();
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
        engineState.imguiManager = std::make_unique<ImGuiManager>(window, device, renderer.getSwapChainRenderPass(), static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
        engineState.uiManager    = std::make_unique<UIManager>(*engineState.imguiManager);

        engineState.uiManager->setOnSaveScene([this]() {
            std::cout << "Saving scene to scene.json..." << '\n';
            sceneSerializer.serialize("scene.json");
        });
        engineState.uiManager->setOnLoadScene([this]() {
            std::cout << "Loading scene from scene.json..." << '\n';
            // Purge stale Jolt bodies before loading a new scene so no invisible
            // ghost colliders remain from the previous scene.
            if (engineState.joltPhysicsSystem) {
                engineState.joltPhysicsSystem->clear();
            }
            if (sceneSerializer.deserialize("scene.json")) {
                // Physics must always start paused; never auto-run after a scene load.
                engineState.physicsSimulationRunning = false;

                // Reset transient selection state to avoid dangling entt entity references
                engineState.selectedEntity = entt::null;
                selectedObjectId           = 0;
                engineState.cameraEntity   = entt::null;

                // get the first camera entity in the loaded scene
                auto const& registry = engineState.scene.getRegistry();
                auto        view     = registry.view<engine::CameraComponent>();
                for (auto entity : view) {
                    std::cout << "[App] Found camera entity in loaded scene\n";
                    engineState.cameraEntity = entity;
                    break;
                }

                // if there is no camera, create a default one
                if (engineState.cameraEntity == entt::null) {
                    std::cout << "[App] Creating default camera for the scene\n";
                    engineState.cameraEntity = engineState.scene.createEntity();
                    engineState.scene.getRegistry().emplace<TransformComponent>(engineState.cameraEntity);
                    engineState.scene.getRegistry().emplace<NameComponent>(engineState.cameraEntity, "Camera");
                    engineState.scene.getRegistry().emplace<CameraComponent>(engineState.cameraEntity);
                }

                std::cout << "[App] Setting loaded camera as active camera\n";

                std::cout << "[App] Successfully deserialized scene.json\n";
            } else {
                std::cout << "[App] Failed to deserialize scene.json\n";
            }

            pendingUpdateCameraAfterSceneLoad = true;
        });
        engineState.uiManager->addPanel(std::make_unique<ScenePanel>(device, &engineState));
        engineState.uiManager->addPanel(std::make_unique<InspectorPanel>(engineState.getScene(), &engineState.physicsSimulationRunning, &engineState.showColliderWireframes));
        engineState.uiManager->addPanel(std::make_unique<SettingsPanel>(&engineState, multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode));
    }

    void App::setupRenderGraph() {
        auto graph = std::make_unique<RenderGraph>();

        // 1. Update Pass
        graph->addPass(std::make_unique<UpdatePass>(&engineState, renderer));

        // 2. Compute Pass
        graph->addPass(std::make_unique<ComputePass>(&engineState));

        // 3. Shadow Pass (EngineState-driven)
        graph->addPass(std::make_unique<ShadowPass>(&engineState));

        // 4. Depth Prepass (Offscreen Depth Only)
        graph->addPass(std::make_unique<DepthPrepass>(&engineState, renderer));

        // 5. Offscreen Pass (Main Scene - Load depth from prepass)
        graph->addPass(std::make_unique<OffscreenPass>(renderer, &engineState, device, debugMode));

        // 6. Composition Pass (PostProcess + UI)
        graph->addPass(std::make_unique<CompositionPass>(renderer, &engineState, *camera, window));

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

    void App::update(float /*frameTime*/) {
        auto resources = engineState.resourceState();
        if (resources.resourceManager != nullptr) {
            resources.resourceManager->updateAsyncCallbacks();
        }

        if (pendingUpdateCameraAfterSceneLoad) {
            pendingUpdateCameraAfterSceneLoad = false;

            // After a scene load, assign to engineState.cameraEntity the CameraComponent of the first found camera entity in the loaded scene.
            auto sceneState          = engineState.sceneState();
            *sceneState.cameraEntity = entt::null;
            auto const& registry     = sceneState.scene->getRegistry();
            auto        view         = registry.view<engine::CameraComponent>();
            for (auto entity : view) {
                *sceneState.cameraEntity = entity;
                break;
            }
        }

        if (auto* scenePanel = engineState.uiManager->getPanel<ScenePanel>()) {
            scenePanel->processDelayedDeletions(engineState.selectedEntity, selectedObjectId);
        }

        // On-demand environment: only load skybox + generate IBL when the user enables skybox display.
        auto rendering  = engineState.renderingState();
        auto sceneState = engineState.sceneState();

        if ((rendering.showSkybox != nullptr) && *rendering.showSkybox && (sceneState.skybox == nullptr)) {
            std::cout << "[App] Loading skybox..." << '\n';
            engineState.skybox = Skybox::loadFromFolder(device, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");

            // Preferred path: load prebaked IBL (offline-generated) instead of regenerating at runtime.
            if (!rendering.iblSystem->loadFromDisk(std::string(TEXTURE_PATH) + "/ibl/Yokohama")) {
                std::cout << "[App] No prebaked IBL found for Yokohama (assets/textures/ibl/Yokohama). Using fallback until you regenerate/bake." << '\n';
            }
        }

        // If the user turns off the skybox, also drop IBL back to fallback.
        if ((rendering.showSkybox != nullptr) && !*rendering.showSkybox && (sceneState.skybox != nullptr)) {
            std::cout << "[App] Skybox disabled. Resetting IBL to fallback." << '\n';
            engineState.skybox.reset();
            rendering.iblSystem->resetToFallback();
        }

        rendering.iblSystem->update();

        // If IBL images/samplers changed (fallback -> generated, or regeneration), refresh descriptor sets.
        uint64_t const newGen = rendering.iblSystem->getGenerationCounter();
        if (newGen != iblGenerationCounter) {
            auto irradianceInfo = rendering.iblSystem->getIrradianceDescriptorInfo();
            auto prefilterInfo  = rendering.iblSystem->getPrefilteredDescriptorInfo();
            auto brdfInfo       = rendering.iblSystem->getBRDFLUTDescriptorInfo();

            auto& deferredIblSets = engineState.deferredIblDescriptorSetsRef();
            for (auto& deferredIblDescriptorSet : deferredIblSets) {
                DescriptorWriter(*engineState.deferredIblSetLayout, *engineState.deferredIblPool).writeImage(0, &irradianceInfo).writeImage(1, &prefilterInfo).writeImage(2, &brdfInfo).overwrite(deferredIblDescriptorSet);
            }

            iblGenerationCounter = newGen;
        }
    }

    void App::render(float frameTime) {
        if (auto commandBuffer = renderer.beginFrame()) {
            if (renderer.wasSwapChainRecreated()) {
                // PostProcessingSystem lives in EngineState — recreate via EngineState if needed.
                engineState.postProcessingSystem = std::make_unique<PostProcessingSystem>(device, renderer.getSwapChainRenderPass(), std::vector<VkDescriptorSetLayout>{engineState.postProcessSetLayoutRef().getDescriptorSetLayout()});
            }

            int const frameIndex = renderer.getFrameIndex();

            FrameInfo frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = *camera,
                .globalDescriptorSet = engineState.renderContext->getGlobalDescriptorSet(frameIndex),
                .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
                .scene               = &engineState.scene,
                .selectedObjectId    = selectedObjectId,
                .selectedEntity      = engineState.selectedEntity,
                .cameraEntity        = engineState.cameraEntity,
                .morphManager        = engineState.animationSystem->getMorphManager(),
                .extent              = renderer.getSwapChainExtent(),
                .debugMode           = debugMode,
            };

            renderPipeline->execute(frameInfo);

            selectedObjectId           = frameInfo.selectedObjectId;
            engineState.selectedEntity = frameInfo.selectedEntity;
            engineState.cameraEntity   = frameInfo.cameraEntity;

            renderer.endFrame();
        }
    }

}  // namespace engine
