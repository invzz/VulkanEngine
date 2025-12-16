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
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
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
    init();
  }

  App::~App() = default;

  void App::init()
  {
    // 1. Setup Render Context
    VkDescriptorImageInfo hzbInfo = renderer.getDepthImageInfo(0);
    renderContext                 = std::make_unique<RenderContext>(device, resourceManager.getMeshManager(), hzbInfo);

    // 2. Setup Scene & Camera
    setupScene();

    // 3. Setup Systems
    setupSystems();

    // 4. Setup UI
    setupUI();

    // 5. Setup Render Graph
    setupRenderGraph();
  }

  void App::setupScene()
  {
    camera   = std::make_unique<Camera>();
    keyboard = std::make_unique<Keyboard>(window);
    mouse    = std::make_unique<Mouse>(window);

    cameraEntity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(cameraEntity);
    scene.getRegistry().emplace<NameComponent>(cameraEntity, "Camera");
    scene.getRegistry().get<TransformComponent>(cameraEntity).translation = {0.0f, -0.2f, -2.5f};
    scene.getRegistry().emplace<CameraComponent>(cameraEntity);

    // Create Sun
    auto sunEntity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(sunEntity);
    scene.getRegistry().emplace<NameComponent>(sunEntity, "Sun");
    scene.getRegistry().emplace<DirectionalLightComponent>(sunEntity);

    // Load Skybox
    std::cout << "[App] Loading skybox..." << std::endl;
    skybox = Skybox::loadFromFolder(device, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");
  }

  void App::setupSystems()
  {
    // Update Systems
    objectSelectionSystem = std::make_unique<ObjectSelectionSystem>(*keyboard);
    inputSystem           = std::make_unique<InputSystem>(*keyboard, *mouse, window);
    cameraSystem          = std::make_unique<CameraSystem>(device, renderer.getOffscreenRenderPass(), renderContext->getGlobalSetLayout());

    // Compute Systems
    animationSystem = std::make_unique<AnimationSystem>(device);
    lodSystem       = std::make_unique<LODSystem>();

    // Shadow & IBL
    shadowSystem = std::make_unique<ShadowSystem>(device, 2048);
    iblSystem    = std::make_unique<IBLSystem>(device);

    std::cout << "[App] Generating IBL maps..." << std::endl;
    iblSystem->generateFromSkybox(*skybox);

    // Render Systems
    std::cout << "[App] Creating render systems..." << std::endl;
    skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(device, renderer.getOffscreenRenderPass());
    meshRenderSystem   = std::make_unique<MeshRenderSystem>(device,
                                                          renderer.getOffscreenRenderPass(),
                                                          renderContext->getGlobalSetLayout(),
                                                          resourceManager.getTextureManager().getDescriptorSetLayout());
    lightSystem        = std::make_unique<LightSystem>(device, renderer.getOffscreenRenderPass(), renderContext->getGlobalSetLayout());

    meshRenderSystem->setShadowSystem(shadowSystem.get());
    meshRenderSystem->setIBLSystem(iblSystem.get());

    // Post Processing
    postProcessPool = DescriptorPool::Builder(device)
                              .setMaxSets(SwapChain::maxFramesInFlight())
                              .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 2)
                              .build();

    postProcessSetLayout = DescriptorSetLayout::Builder(device)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build();

    postProcessingSystem = std::make_unique<PostProcessingSystem>(device,
                                                                  renderer.getSwapChainRenderPass(),
                                                                  std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});

    postProcessDescriptorSets.resize(SwapChain::maxFramesInFlight());
    for (int i = 0; i < postProcessDescriptorSets.size(); i++)
    {
      auto imageInfo = renderer.getOffscreenImageInfo(i);
      auto depthInfo = renderer.getDepthImageInfo(i);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).writeImage(1, &depthInfo).build(postProcessDescriptorSets[i]);
    }
  }

  void App::setupUI()
  {
    imguiManager = std::make_unique<ImGuiManager>(window, device, renderer.getSwapChainRenderPass(), static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
    uiManager    = std::make_unique<UIManager>(*imguiManager);

    uiManager->setOnSaveScene([this]() {
      std::cout << "Saving scene to scene.json..." << std::endl;
      sceneSerializer.serialize("scene.json");
    });
    uiManager->setOnLoadScene([this]() {
      std::cout << "Loading scene from scene.json..." << std::endl;
      sceneSerializer.deserialize("scene.json");
    });

    uiManager->addPanel(std::make_unique<ModelImportPanel>(device, scene, *animationSystem, resourceManager));
    uiManager->addPanel(std::make_unique<ScenePanel>(device, scene, *animationSystem));
    uiManager->addPanel(std::make_unique<InspectorPanel>(scene));
    uiManager->addPanel(std::make_unique<SettingsPanel>(cameraEntity, &scene, *iblSystem, *skybox, skySettings, timeOfDay, postProcessPush, debugMode));
  }

  void App::setupRenderGraph()
  {
    renderGraph = std::make_unique<RenderGraph>();

    // 1. Update Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Update", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = *objectSelectionSystem,
              .inputSystem           = *inputSystem,
              .cameraSystem          = *cameraSystem,
              .animationSystem       = *animationSystem,
              .lodSystem             = *lodSystem,
              .meshRenderSystem      = *meshRenderSystem,
              .lightSystem           = *lightSystem,
              .shadowSystem          = *shadowSystem,
              .skyboxRenderSystem    = *skyboxRenderSystem,
              .renderContext         = *renderContext,
              .uiManager             = *uiManager,
              .skybox                = skybox.get(),
              .skySettings           = skySettings,
      };
      updatePhase(frameInfo, state);
    }));

    // 2. Compute Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Compute", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = *objectSelectionSystem,
              .inputSystem           = *inputSystem,
              .cameraSystem          = *cameraSystem,
              .animationSystem       = *animationSystem,
              .lodSystem             = *lodSystem,
              .meshRenderSystem      = *meshRenderSystem,
              .lightSystem           = *lightSystem,
              .shadowSystem          = *shadowSystem,
              .skyboxRenderSystem    = *skyboxRenderSystem,
              .renderContext         = *renderContext,
              .uiManager             = *uiManager,
              .skybox                = skybox.get(),
              .skySettings           = skySettings,
      };
      computePhase(frameInfo, state);
    }));

    // 3. Shadow Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Shadow", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = *objectSelectionSystem,
              .inputSystem           = *inputSystem,
              .cameraSystem          = *cameraSystem,
              .animationSystem       = *animationSystem,
              .lodSystem             = *lodSystem,
              .meshRenderSystem      = *meshRenderSystem,
              .lightSystem           = *lightSystem,
              .shadowSystem          = *shadowSystem,
              .skyboxRenderSystem    = *skyboxRenderSystem,
              .renderContext         = *renderContext,
              .uiManager             = *uiManager,
              .skybox                = skybox.get(),
              .skySettings           = skySettings,
      };
      shadowPhase(frameInfo, state);
    }));

    // 4. Offscreen Pass (Main Scene)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Offscreen", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = *objectSelectionSystem,
              .inputSystem           = *inputSystem,
              .cameraSystem          = *cameraSystem,
              .animationSystem       = *animationSystem,
              .lodSystem             = *lodSystem,
              .meshRenderSystem      = *meshRenderSystem,
              .lightSystem           = *lightSystem,
              .shadowSystem          = *shadowSystem,
              .skyboxRenderSystem    = *skyboxRenderSystem,
              .renderContext         = *renderContext,
              .uiManager             = *uiManager,
              .skybox                = skybox.get(),
              .skySettings           = skySettings,
      };
      renderer.beginOffscreenRenderPass(frameInfo.commandBuffer);
      renderScenePhase(frameInfo, state);
      renderer.endOffscreenRenderPass(frameInfo.commandBuffer);

      renderer.generateOffscreenMipmaps(frameInfo.commandBuffer);
      renderer.generateDepthPyramid(frameInfo.commandBuffer);
    }));

    // 5. Composition Pass (PostProcess + UI)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Composition", [&](FrameInfo& frameInfo) {
      GameLoopState state{
              .objectSelectionSystem = *objectSelectionSystem,
              .inputSystem           = *inputSystem,
              .cameraSystem          = *cameraSystem,
              .animationSystem       = *animationSystem,
              .lodSystem             = *lodSystem,
              .meshRenderSystem      = *meshRenderSystem,
              .lightSystem           = *lightSystem,
              .shadowSystem          = *shadowSystem,
              .skyboxRenderSystem    = *skyboxRenderSystem,
              .renderContext         = *renderContext,
              .uiManager             = *uiManager,
              .skybox                = skybox.get(),
              .skySettings           = skySettings,
      };

      renderer.beginSwapChainRenderPass(frameInfo.commandBuffer);

      auto imageInfo = renderer.getOffscreenImageInfo(frameInfo.frameIndex);
      auto depthInfo = renderer.getDepthImageInfo(frameInfo.frameIndex);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool)
              .writeImage(0, &imageInfo)
              .writeImage(1, &depthInfo)
              .overwrite(postProcessDescriptorSets[frameInfo.frameIndex]);

      postProcessPush.inverseProjection = glm::inverse(camera->getProjection());
      postProcessPush.projection        = camera->getProjection();

      postProcessingSystem->render(frameInfo, postProcessDescriptorSets[frameInfo.frameIndex], postProcessPush);

      uiPhase(frameInfo, frameInfo.commandBuffer, state);
      renderer.endSwapChainRenderPass(frameInfo.commandBuffer);
    }));
  }

  void App::run()
  {
    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose())
    {
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

  void App::update(float frameTime)
  {
    if (auto* scenePanel = uiManager->getPanel<ScenePanel>())
    {
      scenePanel->processDelayedDeletions(selectedEntity, selectedObjectId);
    }

    iblSystem->update();

    // Update Sun
    if (skySettings.useProcedural)
    {
      // timeOfDay += frameTime * daySpeed; // Auto-advance or let UI control it

      // Calculate sun direction (Simple orbit)
      // 0 = Noon (Top), PI/2 = Sunset, PI = Midnight, 3PI/2 = Sunrise
      // We add PI to align 0 with Noon (Visual Top)
      float     sunAngle = timeOfDay + glm::pi<float>();
      glm::vec3 sunPos   = glm::vec3(0.0f, glm::cos(sunAngle), glm::sin(sunAngle));

      // The procedural sky shader has an inverted Y coordinate system.
      // To make the sun appear at the correct height, we need to flip the Y component
      // of the direction vector passed to the shader.
      glm::vec3 shaderSunDir   = sunPos;
      shaderSunDir.y           = -shaderSunDir.y;
      skySettings.sunDirection = glm::vec4(shaderSunDir, 1.0f);

      // Simple color change based on height
      // Visual height is inverted relative to sunPos.y due to the shader flip
      float height       = -sunPos.y;
      float sunIntensity = 0.0f;

      if (height > 0.2f)
      {
        skySettings.sunColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Day
        sunIntensity         = 1.0f;
      }
      else if (height > -0.2f)
      {
        // Lerp to orange
        float t              = (height + 0.2f) / 0.4f;
        skySettings.sunColor = glm::mix(glm::vec4(1.0f, 0.4f, 0.1f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), t);
        sunIntensity         = glm::mix(0.0f, 1.0f, t);
      }
      else
      {
        skySettings.sunColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Night
        sunIntensity         = 0.0f;
      }

      skySettings.sunDirection.w = sunIntensity;
      // Update Directional Light in Scene
      auto view = scene.getRegistry().view<DirectionalLightComponent, TransformComponent>();
      for (auto entity : view)
      {
        auto& transform = view.get<TransformComponent>(entity);
        auto& light     = view.get<DirectionalLightComponent>(entity);

        // Update light color/intensity
        light.color     = glm::vec3(skySettings.sunColor);
        light.intensity = skySettings.sunDirection.w;

        // Update rotation (Light direction = -SunPos)
        // We can reconstruct SunPos from skySettings by flipping Y back, or just use -skySettings with flipped Y
        glm::vec3 lightDir = -glm::vec3(skySettings.sunDirection);
        lightDir.y         = -lightDir.y; // Flip Y back to match world space

        // Convert to Euler (YXZ)
        // Assuming default forward is +Z
        transform.rotation.y = glm::atan(lightDir.x, lightDir.z);
        transform.rotation.x = glm::asin(-lightDir.y);
        transform.rotation.z = 0.0f;
      }

      // Lazy IBL update
      if (glm::distance(skySettings.sunDirection, lastSunDirection) > 0.05f) // Update every ~3 degrees
      {
        // Only update if we have a valid skyboxRenderSystem (it should be valid here)
        if (skyboxRenderSystem)
        {
          iblSystem->generateFromProcedural(*skyboxRenderSystem, skySettings);
          lastSunDirection = skySettings.sunDirection;
        }
      }
    }
  }

  void App::render(float frameTime)
  {
    if (auto commandBuffer = renderer.beginFrame())
    {
      if (renderer.wasSwapChainRecreated())
      {
        postProcessingSystem = std::make_unique<PostProcessingSystem>(device,
                                                                      renderer.getSwapChainRenderPass(),
                                                                      std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});
      }

      int frameIndex = renderer.getFrameIndex();

      int                   prevFrameIndex = (frameIndex - 1 + SwapChain::maxFramesInFlight()) % SwapChain::maxFramesInFlight();
      VkDescriptorImageInfo hzbInfo        = renderer.getDepthImageInfo(prevFrameIndex);
      renderContext->updateHZBDescriptor(frameIndex, hzbInfo);

      FrameInfo frameInfo{
              .frameIndex          = frameIndex,
              .frameTime           = frameTime,
              .commandBuffer       = commandBuffer,
              .camera              = *camera,
              .globalDescriptorSet = renderContext->getGlobalDescriptorSet(frameIndex),
              .globalTextureSet    = resourceManager.getTextureManager().getDescriptorSet(),
              .scene               = &scene,
              .selectedObjectId    = selectedObjectId,
              .selectedEntity      = selectedEntity,
              .cameraEntity        = cameraEntity,
              .morphManager        = animationSystem->getMorphManager(),
              .extent              = renderer.getSwapChainExtent(),
      };

      renderGraph->execute(frameInfo);

      selectedObjectId = frameInfo.selectedObjectId;
      selectedEntity   = frameInfo.selectedEntity;
      cameraEntity     = frameInfo.cameraEntity;

      renderer.endFrame();
    }
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
    state.shadowSystem.renderShadowMaps(frameInfo, 50.0f);

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
    if (state.skybox || state.skySettings.useProcedural)
    {
      state.skyboxRenderSystem.render(frameInfo, state.skybox, state.skySettings);
    }

    state.meshRenderSystem.render(frameInfo);
    state.lightSystem.render(frameInfo);  // Draw light debug visualizations
    state.cameraSystem.render(frameInfo); // Draw camera debug visualizations
  }

  void App::uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state)
  {
    if (window.isCursorVisible())
    {
      state.uiManager.render(frameInfo, commandBuffer);
    }
  }

} // namespace engine