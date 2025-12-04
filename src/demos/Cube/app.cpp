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
#include "3dEngine/TextureManager.hpp"
#include "3dEngine/Window.hpp"
#include "3dEngine/ansi_colors.hpp"

// Systems
#include "3dEngine/IBLSystem.hpp"
#include "3dEngine/RenderGraph.hpp"
#include "3dEngine/systems/AnimationSystem.hpp"
#include "3dEngine/systems/CameraSystem.hpp"
#include "3dEngine/systems/InputSystem.hpp"
#include "3dEngine/systems/LODSystem.hpp"
#include "3dEngine/systems/LightSystem.hpp"
#include "3dEngine/systems/MeshRenderSystem.hpp"
#include "3dEngine/systems/ObjectSelectionSystem.hpp"
#include "3dEngine/systems/PostProcessingSystem.hpp"
#include "3dEngine/systems/ShadowSystem.hpp"
#include "3dEngine/systems/SkyboxRenderSystem.hpp"

// Demo specific
#include "RenderContext.hpp"
#include "SceneLoader.hpp"

// UI Panels
#include "ui/AnimationPanel.hpp"
#include "ui/CameraPanel.hpp"
#include "ui/LightsPanel.hpp"
#include "ui/ModelImportPanel.hpp"
#include "ui/PostProcessPanel.hpp"
#include "ui/ScenePanel.hpp"
#include "ui/TransformPanel.hpp"
#include "ui/UIManager.hpp"

namespace engine {

  App::App()
  {
    SceneLoader::loadScene(device, objectManager, resourceManager);
  }
  App::~App() = default;

  void App::run()
  {
    // ============================================================================
    // INITIALIZATION PHASE
    // ============================================================================

    // Setup rendering context (descriptors, UBO buffers)
    // Creates descriptor layouts and global descriptor sets for shader uniforms
    RenderContext renderContext{device, resourceManager.getMeshManager()};

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
    LODSystem       lodSystem{};             // Level of Detail system

    // Register all animated objects with the animation system
    // This allows the system to track and update only objects that need animation
    for (auto& [id, obj] : objectManager.getAllObjects())
    {
      if (obj.animationController || (obj.model && obj.model->hasMorphTargets()))
      {
        animationSystem.registerAnimatedObject(id);
      }
    }

    // Shadow Mapping (must be before render systems that use it):
    ShadowSystem shadowSystem{device, 2048};

    // IBL System
    IBLSystem iblSystem{device};

    // Load Skybox
    std::cout << "[App] Loading skybox..." << std::endl;
    auto skybox = Skybox::loadFromFolder(device, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");

    // Generate IBL maps
    std::cout << "[App] Generating IBL maps..." << std::endl;
    iblSystem.generateFromSkybox(*skybox);

    // Render Systems:
    std::cout << "[App] Creating render systems..." << std::endl;
    SkyboxRenderSystem skyboxRenderSystem{device, renderer.getOffscreenRenderPass()};
    MeshRenderSystem   meshRenderSystem{device,
                                      renderer.getOffscreenRenderPass(),
                                      renderContext.getGlobalSetLayout(),
                                      resourceManager.getTextureManager().getDescriptorSetLayout()};
    LightSystem        lightSystem{device, renderer.getOffscreenRenderPass(), renderContext.getGlobalSetLayout()};

    // Connect shadow system to PBR render system (supports multiple shadow maps)
    meshRenderSystem.setShadowSystem(&shadowSystem);
    meshRenderSystem.setIBLSystem(&iblSystem);

    // UI System:
    ImGuiManager imguiManager{window, device, renderer.getSwapChainRenderPass(), static_cast<uint32_t>(SwapChain::maxFramesInFlight())};

    // Post Processing System
    auto postProcessPool = DescriptorPool::Builder(device)
                                   .setMaxSets(SwapChain::maxFramesInFlight())
                                   .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight())
                                   .build();

    auto postProcessSetLayout =
            DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();

    PostProcessingSystem postProcessingSystem{device, renderer.getSwapChainRenderPass(), {postProcessSetLayout->getDescriptorSetLayout()}};

    std::vector<VkDescriptorSet> postProcessDescriptorSets(SwapChain::maxFramesInFlight());
    for (int i = 0; i < postProcessDescriptorSets.size(); i++)
    {
      auto imageInfo = renderer.getOffscreenImageInfo(i);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).build(postProcessDescriptorSets[i]);
    }

    // UI Manager with panels
    PostProcessPushConstants postProcessPush{};
    UIManager                uiManager{imguiManager};

    uiManager.setOnSaveScene([this]() {
      std::cout << "Saving scene to scene.json..." << std::endl;
      sceneSerializer.serialize("scene.json");
    });
    uiManager.setOnLoadScene([this]() {
      std::cout << "Loading scene from scene.json..." << std::endl;
      // Clear existing objects first?
      // The deserializer currently appends.
      // Ideally we should clear. But GameObjectManager doesn't have clear().
      // Let's assume for now we just load on top or restart app to clear.
      // Or I should implement clear() in GameObjectManager.
      sceneSerializer.deserialize("scene.json");
    });

    uiManager.addPanel(std::make_unique<ModelImportPanel>(device, objectManager, animationSystem));
    uiManager.addPanel(std::make_unique<CameraPanel>(cameraObject));
    uiManager.addPanel(std::make_unique<TransformPanel>(objectManager));
    uiManager.addPanel(std::make_unique<LightsPanel>(objectManager));
    uiManager.addPanel(std::make_unique<AnimationPanel>(objectManager));
    uiManager.addPanel(std::make_unique<ScenePanel>(device, objectManager, animationSystem));
    uiManager.addPanel(std::make_unique<PostProcessPanel>(postProcessPush));

    // Selection state (persisted across frames)
    GameObject::id_t selectedObjectId = 0;
    GameObject*      selectedObject   = nullptr;

    // Timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto lastFpsTime = currentTime;
    int  frameCount  = 0;

    // ============================================================================
    // RENDER GRAPH SETUP
    // ============================================================================
    RenderGraph renderGraph;

    // 1. Update Pass
    renderGraph.addPass(std::make_unique<LambdaRenderPass>("Update", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = objectSelectionSystem,
              .inputSystem           = inputSystem,
              .cameraSystem          = cameraSystem,
              .animationSystem       = animationSystem,
              .lodSystem             = lodSystem,
              .meshRenderSystem      = meshRenderSystem,
              .lightSystem           = lightSystem,
              .shadowSystem          = shadowSystem,
              .skyboxRenderSystem    = skyboxRenderSystem,
              .renderContext         = renderContext,
              .uiManager             = uiManager,
              .skybox                = skybox.get(),
      };
      updatePhase(frameInfo, state);
    }));

    // 2. Compute Pass
    renderGraph.addPass(std::make_unique<LambdaRenderPass>("Compute", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = objectSelectionSystem,
              .inputSystem           = inputSystem,
              .cameraSystem          = cameraSystem,
              .animationSystem       = animationSystem,
              .lodSystem             = lodSystem,
              .meshRenderSystem      = meshRenderSystem,
              .lightSystem           = lightSystem,
              .shadowSystem          = shadowSystem,
              .skyboxRenderSystem    = skyboxRenderSystem,
              .renderContext         = renderContext,
              .uiManager             = uiManager,
              .skybox                = skybox.get(),
      };
      computePhase(frameInfo, state);
    }));

    // 3. Shadow Pass
    renderGraph.addPass(std::make_unique<LambdaRenderPass>("Shadow", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = objectSelectionSystem,
              .inputSystem           = inputSystem,
              .cameraSystem          = cameraSystem,
              .animationSystem       = animationSystem,
              .lodSystem             = lodSystem,
              .meshRenderSystem      = meshRenderSystem,
              .lightSystem           = lightSystem,
              .shadowSystem          = shadowSystem,
              .skyboxRenderSystem    = skyboxRenderSystem,
              .renderContext         = renderContext,
              .uiManager             = uiManager,
              .skybox                = skybox.get(),
      };
      shadowPhase(frameInfo, state);
    }));

    // 4. Offscreen Pass (Main Scene)
    renderGraph.addPass(std::make_unique<LambdaRenderPass>("Offscreen", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = objectSelectionSystem,
              .inputSystem           = inputSystem,
              .cameraSystem          = cameraSystem,
              .animationSystem       = animationSystem,
              .lodSystem             = lodSystem,
              .meshRenderSystem      = meshRenderSystem,
              .lightSystem           = lightSystem,
              .shadowSystem          = shadowSystem,
              .skyboxRenderSystem    = skyboxRenderSystem,
              .renderContext         = renderContext,
              .uiManager             = uiManager,
              .skybox                = skybox.get(),
      };
      renderer.beginOffscreenRenderPass(frameInfo.commandBuffer);
      renderScenePhase(frameInfo, state);
      renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
      renderer.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }));

    // 5. Composition Pass (PostProcess + UI)
    renderGraph.addPass(std::make_unique<LambdaRenderPass>("Composition", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = objectSelectionSystem,
              .inputSystem           = inputSystem,
              .cameraSystem          = cameraSystem,
              .animationSystem       = animationSystem,
              .lodSystem             = lodSystem,
              .meshRenderSystem      = meshRenderSystem,
              .lightSystem           = lightSystem,
              .shadowSystem          = shadowSystem,
              .skyboxRenderSystem    = skyboxRenderSystem,
              .renderContext         = renderContext,
              .uiManager             = uiManager,
              .skybox                = skybox.get(),
      };

      renderer.beginSwapChainRenderPass(frameInfo.commandBuffer);

      // Update descriptor set for current frame
      auto imageInfo = renderer.getOffscreenImageInfo(frameInfo.frameIndex);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).overwrite(postProcessDescriptorSets[frameInfo.frameIndex]);

      postProcessingSystem.render(frameInfo, postProcessDescriptorSets[frameInfo.frameIndex], postProcessPush);

      uiPhase(frameInfo, frameInfo.commandBuffer, state);
      renderer.endSwapChainRenderPass(frameInfo.commandBuffer);
    }));

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
                .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
                .objectManager       = &objectManager,
                .selectedObjectId    = selectedObjectId,
                .selectedObject      = selectedObject,
                .cameraObject        = cameraObject,
                .morphManager        = animationSystem.getMorphManager(),
                .extent              = renderer.getSwapChainExtent(),
        };

        // Execute Render Graph
        renderGraph.execute(frameInfo);

        // Persist selection state across frames (from FrameInfo back to local variables)
        // Note: This needs to happen after the graph execution because UI/Selection systems modify it
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
    state.lodSystem.update(frameInfo);                               // Update Level of Detail
    state.cameraSystem.update(frameInfo, renderer.getAspectRatio()); // Update camera matrices
  }

  void App::computePhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update all animations (BEFORE render pass)
    // - Updates AnimationControllers (interpolates morph weights, skeletal transforms)
    // - Dispatches compute shaders for morph targets: baseVertices + deltas * weights → blended
    state.animationSystem.update(frameInfo);
  }

  void App::shadowPhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update uniform buffer with per-frame data FIRST (this also rotates point lights)
    GlobalUbo ubo{};

    state.lightSystem.update(frameInfo, ubo); // Update light positions in UBO (rotates them)

    // Render shadow maps for all shadow-casting lights (after positions are updated)
    state.shadowSystem.renderShadowMaps(frameInfo, 30.0f);

    ubo.projection       = frameInfo.camera.getProjection();
    ubo.view             = frameInfo.camera.getView();
    ubo.cameraPosition   = glm::vec4(frameInfo.cameraObject.transform.translation, 1.0f);
    ubo.shadowLightCount = state.shadowSystem.getShadowLightCount();

    // Copy all light space matrices
    for (int i = 0; i < ubo.shadowLightCount; i++)
    {
      ubo.lightSpaceMatrices[i] = state.shadowSystem.getLightSpaceMatrix(i);
    }

    // Copy cube shadow map data for point lights
    ubo.cubeShadowLightCount = state.shadowSystem.getCubeShadowLightCount();
    for (int i = 0; i < ubo.cubeShadowLightCount && i < 4; i++)
    {
      ubo.pointLightShadowData[i] = glm::vec4(state.shadowSystem.getPointLightPosition(i), state.shadowSystem.getPointLightRange(i));
    }

    state.renderContext.updateUBO(frameInfo.frameIndex, ubo);
  }

  void App::renderScenePhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // RENDER SYSTEMS - These issue actual draw calls

    // Render skybox first (renders at z=1.0, behind everything)
    if (state.skybox)
    {
      state.skyboxRenderSystem.render(frameInfo, *state.skybox);
    }

    state.meshRenderSystem.render(frameInfo);
    state.lightSystem.render(frameInfo); // Draw light debug visualizations
  }

  void App::uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state)
  {
    state.uiManager.render(frameInfo, commandBuffer);
  }

} // namespace engine