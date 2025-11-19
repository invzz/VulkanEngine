#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "app.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <chrono>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

#include "3dEngine/AnimationController.hpp"
#include "3dEngine/Camera.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/ImGuiManager.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Mouse.hpp"
#include "3dEngine/Window.hpp"
#include "3dEngine/ansi_colors.hpp"

// Systems
#include "3dEngine/systems/AnimationSystem.hpp"
#include "3dEngine/systems/CameraSystem.hpp"
#include "3dEngine/systems/InputSystem.hpp"
#include "3dEngine/systems/LightSystem.hpp"
#include "3dEngine/systems/ObjectSelectionSystem.hpp"
#include "3dEngine/systems/PBRRenderSystem.hpp"

// Demo specific
#include "RenderContext.hpp"
#include "SceneLoader.hpp"

// UI Panels
#include "ui/AnimationPanel.hpp"
#include "ui/CameraPanel.hpp"
#include "ui/LightsPanel.hpp"
#include "ui/ModelImportPanel.hpp"
#include "ui/ScenePanel.hpp"
#include "ui/TransformPanel.hpp"
#include "ui/UIManager.hpp"

namespace engine {

  App::App()
  {
    SceneLoader::loadScene(device, gameObjects);
  }
  App::~App() = default;

  void App::run()
  {
    // ============================================================================
    // INITIALIZATION PHASE
    // ============================================================================

    // Setup rendering context (descriptors, UBO buffers)
    // Creates descriptor layouts and global descriptor sets for shader uniforms
    RenderContext renderContext{device};

    // Setup scene objects (camera, input devices)
    Camera   camera{};
    Keyboard keyboard{window};
    Mouse    mouse{window};

    GameObject cameraObject            = GameObject::create();
    cameraObject.transform.translation = {0.0f, -0.2f, -2.5f};

    // ============================================================================
    // SYSTEMS INITIALIZATION
    // ============================================================================
    // Systems are organized into three categories:
    // 1. UPDATE SYSTEMS - Process input, physics, animations (update phase)
    // 2. COMPUTE SYSTEMS - GPU compute shaders for data preparation (compute phase)
    // 3. RENDER SYSTEMS - Issue GPU draw calls (render phase)

    // Update Systems:
    ObjectSelectionSystem objectSelectionSystem{keyboard};      // Handles object picking with mouse
    InputSystem           inputSystem{keyboard, mouse, window}; // Camera movement and controls
    CameraSystem          cameraSystem{};                       // Camera matrix calculations

    // Compute Systems:
    AnimationSystem animationSystem{device}; // Animations: morph targets, skeletal, procedural

    // Register all animated objects with the animation system
    // This allows the system to track and update only objects that need animation
    for (auto& [id, obj] : gameObjects)
    {
      if (obj.animationController || (obj.model && obj.model->hasMorphTargets()))
      {
        animationSystem.registerAnimatedObject(id);
      }
    }

    // Shadow Mapping (must be before render systems that use it):
    // Shadow system removed - to be reimplemented later

    // Render Systems:
    std::cout << "[App] Creating PBRRenderSystem..." << std::endl;
    PBRRenderSystem pbrRenderSystem{device, renderer.getSwapChainRenderPass(), renderContext.getGlobalSetLayout()};
    LightSystem     lightSystem{device, renderer.getSwapChainRenderPass(), renderContext.getGlobalSetLayout()};

    // UI System:
    ImGuiManager imguiManager{window, device, renderer.getSwapChainRenderPass(), static_cast<uint32_t>(SwapChain::maxFramesInFlight())};

    // UI Manager with panels
    UIManager uiManager{imguiManager};
    uiManager.addPanel(std::make_unique<ModelImportPanel>(device, gameObjects, animationSystem, pbrRenderSystem));
    uiManager.addPanel(std::make_unique<CameraPanel>(cameraObject));
    uiManager.addPanel(std::make_unique<TransformPanel>(gameObjects));
    uiManager.addPanel(std::make_unique<LightsPanel>(gameObjects));
    uiManager.addPanel(std::make_unique<AnimationPanel>(gameObjects));
    uiManager.addPanel(std::make_unique<ScenePanel>(device, gameObjects, animationSystem));

    // Selection state (persisted across frames)
    GameObject::id_t selectedObjectId = 0;
    GameObject*      selectedObject   = nullptr;

    // Timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto lastFpsTime = currentTime;
    int  frameCount  = 0;

    // ============================================================================
    // MAIN GAME LOOP
    // ============================================================================
    while (!window.shouldClose())
    {
      glfwPollEvents();

      // Process delayed deletions at the start of the frame (before any rendering)
      if (auto* scenePanel = uiManager.getPanel<ScenePanel>())
      {
        scenePanel->processDelayedDeletions();
      }

      // Calculate frame time (delta time)
      auto  newTime   = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
      currentTime     = newTime;
      frameTime       = glm::min(frameTime, 0.1f); // Cap at 100ms to prevent large jumps

      // Begin frame - get command buffer for recording GPU commands
      if (auto commandBuffer = renderer.beginFrame())
      {
        int frameIndex = renderer.getFrameIndex();

        // Build frame info structure (passed to all systems)
        // Contains all per-frame state: time, camera, selected objects, etc.
        FrameInfo frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = camera,
                .globalDescriptorSet = renderContext.getGlobalDescriptorSet(frameIndex),
                .gameObjects         = gameObjects,
                .selectedObjectId    = selectedObjectId,
                .selectedObject      = selectedObject,
                .cameraObject        = cameraObject,
                .morphManager        = animationSystem.getMorphManager(),
        };

        // Build game loop state (references to all systems and state)
        GameLoopState state{
                .objectSelectionSystem = objectSelectionSystem,
                .inputSystem           = inputSystem,
                .cameraSystem          = cameraSystem,
                .animationSystem       = animationSystem,
                .pbrRenderSystem       = pbrRenderSystem,
                .lightSystem           = lightSystem,
                .renderContext         = renderContext,
                .uiManager             = uiManager,
        };

        // ========================================================================
        // UPDATE PHASE - Process game logic, input, animations
        // ========================================================================
        updatePhase(frameInfo, state);

        // Persist selection state across frames (from FrameInfo back to local variables)
        selectedObjectId = frameInfo.selectedObjectId;
        selectedObject   = frameInfo.selectedObject;

        // ========================================================================
        // COMPUTE PHASE - Animations and GPU compute shaders
        // ========================================================================
        computePhase(frameInfo, state);

        // Shadow phase removed - to be reimplemented later

        // ========================================================================
        // RENDER PHASE - Issue GPU draw calls (including UI)
        // ========================================================================
        renderer.beginSwapChainRenderPass(commandBuffer);
        renderPhase(frameInfo, state);
        uiPhase(frameInfo, commandBuffer, state);
        renderer.endSwapChainRenderPass(commandBuffer);

        // Persist selection state after UI phase (UI can change selection)
        selectedObjectId = frameInfo.selectedObjectId;
        selectedObject   = frameInfo.selectedObject;

        renderer.endFrame();
      }
    }

    // Wait for GPU to finish all work before cleanup
    device.WaitIdle();
  }

  void App::updatePhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update systems (CPU-side processing)
    state.objectSelectionSystem.update(frameInfo);                   // Handle object selection with mouse
    state.inputSystem.update(frameInfo);                             // Process keyboard/mouse input
    state.cameraSystem.update(frameInfo, renderer.getAspectRatio()); // Update camera matrices

    // Update uniform buffer with per-frame data
    GlobalUbo ubo{};
    state.lightSystem.update(frameInfo, ubo); // Update light positions in UBO
    ubo.projection     = frameInfo.camera.getProjection();
    ubo.view           = frameInfo.camera.getView();
    ubo.cameraPosition = glm::vec4(frameInfo.cameraObject.transform.translation, 1.0f);

    state.renderContext.updateUBO(frameInfo.frameIndex, ubo);
  }

  void App::computePhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update all animations (BEFORE render pass)
    // - Updates AnimationControllers (interpolates morph weights, skeletal transforms)
    // - Dispatches compute shaders for morph targets: baseVertices + deltas * weights → blended
    state.animationSystem.update(frameInfo);
  }

  // Shadow phase removed - to be reimplemented later

  void App::renderPhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // RENDER SYSTEMS - These issue actual draw calls
    state.pbrRenderSystem.render(frameInfo); // Draw objects with PBR materials (uses blended buffers if available)
    state.lightSystem.render(frameInfo);     // Draw light debug visualizations
  }

  void App::uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state)
  {
    state.uiManager.render(frameInfo, commandBuffer);
  }

} // namespace engine