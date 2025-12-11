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

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/TextureManager.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

// Systems
#include "Engine/Graphics/RenderGraph.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/MeshRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

// Demo specific
#include "RenderContext.hpp"
#include "SceneLoader.hpp"

// UI Panels
#include "ui/InspectorPanel.hpp"
#include "ui/ModelImportPanel.hpp"
#include "ui/ScenePanel.hpp"
#include "ui/SettingsPanel.hpp"
#include "ui/UIManager.hpp"

namespace engine {

  App::App()
  {
    // SceneLoader::loadScene(device, scene, resourceManager);
  }
  App::~App() = default;

  void App::run()
  {
    // ============================================================================
    // INITIALIZATION PHASE
    // ============================================================================

    // Setup rendering context (descriptors, UBO buffers)
    // Creates descriptor layouts and global descriptor sets for shader uniforms
    // We use the offscreen depth buffer (mip 0) as initial HZB info.
    // Ideally we should use a view that covers all mips, but for now this satisfies validation.
    VkDescriptorImageInfo hzbInfo = renderer.getDepthImageInfo(0);

    RenderContext renderContext{device, resourceManager.getMeshManager(), hzbInfo};

    // Setup scene objects (camera, input devices)
    Camera   camera{};
    Keyboard keyboard{window};
    Mouse    mouse{window};

    auto cameraEntity = scene.createEntity();

    scene.getRegistry().emplace<TransformComponent>(cameraEntity);
    scene.getRegistry().emplace<NameComponent>(cameraEntity, "Camera");
    scene.getRegistry().get<TransformComponent>(cameraEntity).translation = {0.0f, -0.2f, -2.5f};
    scene.getRegistry().emplace<CameraComponent>(cameraEntity);

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
    // (Registration is now implicit via EnTT views in AnimationSystem::update)

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
                                   .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 2)
                                   .build();

    auto postProcessSetLayout = DescriptorSetLayout::Builder(device)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                        .build();

    auto postProcessingSystem = std::make_unique<PostProcessingSystem>(device,
                                                                       renderer.getSwapChainRenderPass(),
                                                                       std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});

    std::vector<VkDescriptorSet> postProcessDescriptorSets(SwapChain::maxFramesInFlight());
    for (int i = 0; i < postProcessDescriptorSets.size(); i++)
    {
      auto imageInfo = renderer.getOffscreenImageInfo(i);
      auto depthInfo = renderer.getDepthImageInfo(i);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).writeImage(1, &depthInfo).build(postProcessDescriptorSets[i]);
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

    uiManager.addPanel(std::make_unique<ModelImportPanel>(device, scene, animationSystem, resourceManager));
    uiManager.addPanel(std::make_unique<ScenePanel>(device, scene, animationSystem));
    uiManager.addPanel(std::make_unique<InspectorPanel>(scene));
    uiManager.addPanel(std::make_unique<SettingsPanel>(cameraEntity, &scene, iblSystem, *skybox, postProcessPush, debugMode));

    // Selection state (persisted across frames)
    uint32_t     selectedObjectId = 0;
    entt::entity selectedEntity   = entt::null;

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

      // Always generate mipmaps to ensure proper layout transition (COLOR_ATTACHMENT -> SHADER_READ_ONLY)
      // even if bloom is disabled.
      renderer.generateOffscreenMipmaps(frameInfo.commandBuffer);

      // Generate HZB for Occlusion Culling (Next Frame)
      renderer.generateDepthPyramid(frameInfo.commandBuffer);
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
      auto depthInfo = renderer.getDepthImageInfo(frameInfo.frameIndex);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool)
              .writeImage(0, &imageInfo)
              .writeImage(1, &depthInfo)
              .overwrite(postProcessDescriptorSets[frameInfo.frameIndex]);

      postProcessPush.inverseProjection = glm::inverse(camera.getProjection());
      postProcessPush.projection        = camera.getProjection();

      postProcessingSystem->render(frameInfo, postProcessDescriptorSets[frameInfo.frameIndex], postProcessPush);

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
        scenePanel->processDelayedDeletions(selectedEntity, selectedObjectId);
      }

      // Update IBL if requested (deferred to avoid destroying resources in use)
      iblSystem.update();

      // Calculate frame time (delta time)
      auto  newTime   = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float>(newTime - currentTime).count();
      currentTime     = newTime;
      frameTime       = glm::min(frameTime, 0.1f); // Cap at 100ms to prevent large jumps

      // Begin frame - get command buffer for recording GPU commands
      if (auto commandBuffer = renderer.beginFrame())
      {
        if (renderer.wasSwapChainRecreated())
        {
          postProcessingSystem = std::make_unique<PostProcessingSystem>(device,
                                                                        renderer.getSwapChainRenderPass(),
                                                                        std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});
        }

        int frameIndex = renderer.getFrameIndex();

        // Update HZB descriptor for this frame (using previous frame's depth or current if we had a pre-pass)
        // We generated HZB at the end of LAST frame for THIS frame index (ping-ponging).
        // Wait, frameIndex cycles 0..N.
        // If we generated HZB for frameIndex in the previous cycle, it is available now.
        // We need to get the HZB image info from renderer.
        // We need to expose a method in Renderer to get HZB image info.
        // Let's assume we added getHZBImageInfo(frameIndex).
        // But we didn't add it yet.
        // We can construct it manually if we expose getDepthImage/Sampler.

        // Actually, we need the full pyramid view? No, just the sampler and the base view?
        // The shader samples from the texture.
        // We need a view that covers all mips?
        // The depthImageViews in FrameBuffer cover all mips (levelCount = mipLevels).
        // So we can use that.

        // We need to add getDepthImageInfo to Renderer/FrameBuffer.

        // Update HZB descriptor for this frame (using previous frame's depth)
        int                   prevFrameIndex = (frameIndex - 1 + SwapChain::maxFramesInFlight()) % SwapChain::maxFramesInFlight();
        VkDescriptorImageInfo hzbInfo        = renderer.getDepthImageInfo(prevFrameIndex);
        renderContext.updateHZBDescriptor(frameIndex, hzbInfo);

        // Build frame info structure (passed to all systems)
        // Contains all per-frame state: time, camera, selected objects, etc.
        FrameInfo frameInfo{
                .frameIndex          = frameIndex,
                .frameTime           = frameTime,
                .commandBuffer       = commandBuffer,
                .camera              = camera,
                .globalDescriptorSet = renderContext.getGlobalDescriptorSet(frameIndex),
                .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
                .scene               = &scene,
                .selectedObjectId    = selectedObjectId,
                .selectedEntity      = selectedEntity,
                .cameraEntity        = cameraEntity,
                .morphManager        = animationSystem.getMorphManager(),
                .extent              = renderer.getSwapChainExtent(),
        };

        // Execute Render Graph
        renderGraph.execute(frameInfo);

        // Persist selection state across frames (from FrameInfo back to local variables)
        // Note: This needs to happen after the graph execution because UI/Selection systems modify it
        selectedObjectId = frameInfo.selectedObjectId;
        selectedEntity   = frameInfo.selectedEntity;

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
    ubo.cameraPosition   = glm::vec4(frameInfo.scene->getRegistry().get<TransformComponent>(frameInfo.cameraEntity).translation, 1.0f);
    ubo.shadowLightCount = state.shadowSystem.getShadowLightCount();
    ubo.debugMode        = debugMode;

    // Calculate Frustum Planes for Culling (Normalized)
    glm::mat4 vp   = ubo.projection * ubo.view;
    glm::mat4 vpT  = glm::transpose(vp);
    glm::vec4 row0 = vpT[0];
    glm::vec4 row1 = vpT[1];
    glm::vec4 row2 = vpT[2];
    glm::vec4 row3 = vpT[3];

    ubo.frustumPlanes[0] = row3 + row0; // Left
    ubo.frustumPlanes[1] = row3 - row0; // Right
    ubo.frustumPlanes[2] = row3 + row1; // Bottom
    ubo.frustumPlanes[3] = row3 - row1; // Top
    ubo.frustumPlanes[4] = row2;        // Near
    ubo.frustumPlanes[5] = row3 - row2; // Far

    for (int i = 0; i < 6; i++)
    {
      float len = glm::length(glm::vec3(ubo.frustumPlanes[i]));
      ubo.frustumPlanes[i] /= len;
    }

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
    if (window.isCursorVisible())
    {
      state.uiManager.render(frameInfo, commandBuffer);
    }
  }

} // namespace engine