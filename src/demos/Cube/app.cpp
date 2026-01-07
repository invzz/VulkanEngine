#include "app.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <glm/common.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/TextureManager.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan_core.h"

// Systems
#include "Engine/Graphics/RenderGraph.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

// Demo specific
#include "RenderContext.hpp"

// UI Panels
#include "CubeUI/ui/InspectorPanel.hpp"
#include "CubeUI/ui/ModelImportPanel.hpp"
#include "CubeUI/ui/ScenePanel.hpp"
#include "CubeUI/ui/SettingsPanel.hpp"
#include "CubeUI/ui/UIManager.hpp"

namespace {
  struct SunInfo
  {
    glm::vec3 directionToSun{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float     intensity{0.0f};
    bool      valid{false};
  };

  SunInfo queryPrimaryDirectionalLightSunInfo(engine::Scene const& scene)
  {
    SunInfo info{};

    auto const& registry = scene.getRegistry();
    auto        view     = registry.view<engine::TransformComponent, engine::DirectionalLightComponent>();
    for (auto entity : view)
    {
      auto const& transform = view.get<engine::TransformComponent>(entity);
      auto const& light     = view.get<engine::DirectionalLightComponent>(entity);

      glm::vec3 const lightRayDir = glm::normalize(transform.getForwardDir());
      info.directionToSun         = -lightRayDir;
      info.color                  = light.color;
      info.intensity              = light.intensity;
      info.valid                  = true;
      break;
    }

    return info;
  }
} // namespace

namespace engine {

  GameLoopState App::makeGameLoopState()
  {
    return GameLoopState{
            .objectSelectionSystem = *objectSelectionSystem,
            .inputSystem           = *inputSystem,
            .cameraSystem          = *cameraSystem,
            .animationSystem       = *animationSystem,
            .lodSystem             = *lodSystem,
            .modelRenderSystem     = *modelRenderSystem,
            .lightSystem           = *lightSystem,
            .shadowSystem          = *shadowSystem,
            .skyboxRenderSystem    = *skyboxRenderSystem,
            .gridRenderSystem      = *gridRenderSystem,
            .dustRenderSystem      = *dustRenderSystem,
            .renderContext         = *renderContext,
            .uiManager             = *uiManager,
            .skybox                = showSkybox ? skybox.get() : nullptr,
            .showGrid              = showGrid,
            .skySettings           = skySettings,
            .dustSettings          = dustSettings,
    };
  }

  App::App()
  {
    init();
  }

  App::~App() = default;

  void App::init()
  {
    // 1. Setup Render Context
    VkDescriptorImageInfo const hzbInfo = renderer.getHzbImageInfo(0);
    renderContext                       = std::make_unique<RenderContext>(device, resourceManager.getMeshManager(), hzbInfo);

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
  }

  void App::setupSystems()
  {
    // Update Systems
    objectSelectionSystem = std::make_unique<ObjectSelectionSystem>(*keyboard);
    inputSystem           = std::make_unique<InputSystem>(*keyboard, *mouse, window);
    cameraSystem          = std::make_unique<CameraSystem>(device, renderer.getOffscreenRenderPassLoadDepth(), renderContext->getGlobalSetLayout());

    // Compute Systems
    animationSystem = std::make_unique<AnimationSystem>(device);
    lodSystem       = std::make_unique<LODSystem>();

    // Shadow & IBL
    shadowSystem = std::make_unique<ShadowSystem>(device, 2048);
    iblSystem    = std::make_unique<IBLSystem>(device);

    iblGenerationCounter = iblSystem->getGenerationCounter();

    // Render Systems
    std::cout << "[App] Creating render systems..." << '\n';
    skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(device, renderer.getOffscreenRenderPassLoadDepth());
    gridRenderSystem   = std::make_unique<GridRenderSystem>(device, renderer.getOffscreenRenderPassLoadDepth(), renderContext->getGlobalSetLayout());
    dustRenderSystem   = std::make_unique<DustRenderSystem>(device, renderer.getOffscreenRenderPassLoadDepth());
    modelRenderSystem =
            std::make_unique<ModelRenderSystem>(device, renderer.getOffscreenRenderPassLoadDepth(), renderContext->getGlobalSetLayout(), resourceManager.getTextureManager().getDescriptorSetLayout());
    lightSystem = std::make_unique<LightSystem>(device, renderer.getOffscreenRenderPassLoadDepth(), renderContext->getGlobalSetLayout());

    // G-buffer + Deferred lighting
    modelRenderSystem->createGbufferPipeline(renderer.getGbufferRenderPass());

    gbufferPool = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 4).build();

    gbufferSetLayout = DescriptorSetLayout::Builder(device)
                               .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                               .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                               .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                               .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                               .build();

    deferredIblPool = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 3).build();

    deferredIblSetLayout = DescriptorSetLayout::Builder(device)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build();

    deferredShadowPool = DescriptorPool::Builder(device)
                                 .setMaxSets(SwapChain::maxFramesInFlight())
                                 .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * (ShadowSystem::MAX_SHADOW_MAPS + ShadowSystem::MAX_CUBE_SHADOW_MAPS))
                                 .build();

    deferredShadowSetLayout = DescriptorSetLayout::Builder(device)
                                      .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_SHADOW_MAPS)
                                      .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_CUBE_SHADOW_MAPS)
                                      .build();

    deferredLightingSystem = std::make_unique<DeferredLightingSystem>(device,
                                                                      renderer.getDeferredLightingRenderPass(),
                                                                      std::vector<VkDescriptorSetLayout>{renderContext->getGlobalSetLayout(),
                                                                                                         gbufferSetLayout->getDescriptorSetLayout(),
                                                                                                         deferredIblSetLayout->getDescriptorSetLayout(),
                                                                                                         deferredShadowSetLayout->getDescriptorSetLayout()});

    gbufferDescriptorSets.resize(SwapChain::maxFramesInFlight());
    for (int i = 0; i < gbufferDescriptorSets.size(); i++)
    {
      auto nInfo = renderer.getGbufferNormalImageInfo(i);
      auto aInfo = renderer.getGbufferAlbedoImageInfo(i);
      auto mInfo = renderer.getGbufferMaterialImageInfo(i);
      auto dInfo = renderer.getDepthImageInfo(i);

      DescriptorWriter(*gbufferSetLayout, *gbufferPool).writeImage(0, &nInfo).writeImage(1, &aInfo).writeImage(2, &mInfo).writeImage(3, &dInfo).build(gbufferDescriptorSets[i]);
    }

    deferredIblDescriptorSets.resize(SwapChain::maxFramesInFlight());
    for (auto& deferredIblDescriptorSet : deferredIblDescriptorSets)
    {
      auto irradianceInfo = iblSystem->getIrradianceDescriptorInfo();
      auto prefilterInfo  = iblSystem->getPrefilteredDescriptorInfo();
      auto brdfInfo       = iblSystem->getBRDFLUTDescriptorInfo();

      DescriptorWriter(*deferredIblSetLayout, *deferredIblPool).writeImage(0, &irradianceInfo).writeImage(1, &prefilterInfo).writeImage(2, &brdfInfo).build(deferredIblDescriptorSet);
    }

    deferredShadowDescriptorSets.resize(SwapChain::maxFramesInFlight());
    for (auto& deferredShadowDescriptorSet : deferredShadowDescriptorSets)
    {
      if (!deferredShadowPool->allocateDescriptor(deferredShadowSetLayout->getDescriptorSetLayout(), deferredShadowDescriptorSet))
      {
        throw std::runtime_error("Failed to allocate deferred shadow descriptor set");
      }
    }

    // Depth prepass pipeline is created but not scheduled yet (RenderGraph wiring is a follow-up task).
    modelRenderSystem->createDepthPrepassPipeline(renderer.getOffscreenDepthPrepassRenderPass());

    modelRenderSystem->setShadowSystem(shadowSystem.get());
    modelRenderSystem->setIBLSystem(iblSystem.get());

    // Post Processing
    postProcessPool = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 2).build();

    postProcessSetLayout = DescriptorSetLayout::Builder(device)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build();

    postProcessingSystem = std::make_unique<PostProcessingSystem>(device, renderer.getSwapChainRenderPass(), std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});

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
      std::cout << "Saving scene to scene.json..." << '\n';
      sceneSerializer.serialize("scene.json");
    });
    uiManager->setOnLoadScene([this]() {
      std::cout << "Loading scene from scene.json..." << '\n';
      sceneSerializer.deserialize("scene.json");
    });

    uiManager->addPanel(std::make_unique<ModelImportPanel>(device, scene, *animationSystem, resourceManager));
    uiManager->addPanel(std::make_unique<ScenePanel>(device, scene, *animationSystem));
    uiManager->addPanel(std::make_unique<InspectorPanel>(scene));
    uiManager->addPanel(
            std::make_unique<SettingsPanel>(cameraEntity, &scene, *iblSystem, &skybox, showSkybox, showGrid, skySettings, dustSettings, fogSettings, hzbSettings, postProcessPush, debugMode));
  }

  void App::setupRenderGraph()
  {
    renderGraph = std::make_unique<RenderGraph>();

    // 1. Update Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Update", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();
      updatePhase(frameInfo, state);
    }));

    // 2. Compute Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Compute", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();
      computePhase(frameInfo, state);
    }));

    // 3. Shadow Pass
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Shadow", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();
      shadowPhase(frameInfo, state);
    }));

    // 4. Depth Prepass (Offscreen Depth Only)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("DepthPrepass", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();

      renderer.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
      state.modelRenderSystem.renderDepthPrepass(frameInfo);
      renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
    }));

    // 5. HZB Build (same frame, after Depth Prepass)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("HZB", [&](FrameInfo& frameInfo) { renderer.generateDepthPyramid(frameInfo.commandBuffer); }));

    // 6. Offscreen Pass (Main Scene - Load depth from prepass)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Offscreen", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();

      // Reset per-frame dynamic offsets before any mesh passes.
      state.modelRenderSystem.beginFrame(frameInfo.frameIndex);
      state.modelRenderSystem.updateSceneColorDescriptor(frameInfo.frameIndex, renderer.getSceneColorImageInfo(frameInfo.frameIndex));

      // G-buffer descriptors are created once, but the underlying image views/samplers are recreated on window resize.
      // Refresh them every frame to avoid stale handles after swapchain/offscreen resize.
      {
        auto nInfo = renderer.getGbufferNormalImageInfo(frameInfo.frameIndex);
        auto aInfo = renderer.getGbufferAlbedoImageInfo(frameInfo.frameIndex);
        auto mInfo = renderer.getGbufferMaterialImageInfo(frameInfo.frameIndex);
        auto dInfo = renderer.getDepthImageInfo(frameInfo.frameIndex);

        DescriptorWriter(*gbufferSetLayout, *gbufferPool)
                .writeImage(0, &nInfo)
                .writeImage(1, &aInfo)
                .writeImage(2, &mInfo)
                .writeImage(3, &dInfo)
                .overwrite(gbufferDescriptorSets[frameInfo.frameIndex]);
      }

      // Use the "current-frame HZB" global descriptor set for the main scene after the HZB pass.
      // This avoids updating a descriptor set while it is already bound to the command buffer.
      auto const prevGlobalSet      = frameInfo.globalDescriptorSet;
      frameInfo.globalDescriptorSet = renderContext->getGlobalDescriptorSetCurrentHzb(frameInfo.frameIndex);

      // Pass 1: Opaque scene (writes color+depth)
      renderer.beginGbufferRenderPass(frameInfo.commandBuffer);
      state.modelRenderSystem.renderGbuffer(frameInfo);
      renderer.endOffscreenRenderPass(frameInfo.commandBuffer);

      // Pass 2: Deferred lighting (writes HDR color, loads depth)
      renderer.beginDeferredLightingRenderPass(frameInfo.commandBuffer);

      // Shadow descriptors: write the current shadow maps into an array set for deferred lighting.
      {
        int const shadowCount     = state.shadowSystem.getShadowLightCount();
        int const cubeShadowCount = state.shadowSystem.getCubeShadowLightCount();

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
        for (int i = 0; i < shadowCount && i < ShadowSystem::MAX_SHADOW_MAPS; i++)
        {
          shadowInfos[i] = state.shadowSystem.getShadowMapDescriptorInfo(i);
        }
        for (int i = shadowCount; i < ShadowSystem::MAX_SHADOW_MAPS; i++)
        {
          shadowInfos[i] = state.shadowSystem.getShadowMapDescriptorInfo(0);
        }

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
        for (int i = 0; i < cubeShadowCount && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
        {
          cubeShadowInfos[i] = state.shadowSystem.getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cubeShadowCount; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
        {
          cubeShadowInfos[i] = state.shadowSystem.getCubeShadowMapDescriptorInfo(0);
        }

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = deferredShadowDescriptorSets[frameInfo.frameIndex];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = ShadowSystem::MAX_SHADOW_MAPS;
        descriptorWrites[0].pImageInfo      = shadowInfos.data();

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = deferredShadowDescriptorSets[frameInfo.frameIndex];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        descriptorWrites[1].pImageInfo      = cubeShadowInfos.data();

        vkUpdateDescriptorSets(device.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
      }

      deferredLightingSystem->render(frameInfo,
                                     frameInfo.globalDescriptorSet,
                                     gbufferDescriptorSets[frameInfo.frameIndex],
                                     deferredIblDescriptorSets[frameInfo.frameIndex],
                                     deferredShadowDescriptorSets[frameInfo.frameIndex]);
      renderer.endOffscreenRenderPass(frameInfo.commandBuffer);

      // Pass 3: Forward overlays: skybox + transmission (no blending) + alpha-blend (regular transparency)
      // When debug views are enabled, skip overlays so deferred debug output stays visible.
      if (debugMode == 0)
      {
        // 3a) Skybox first, then copy HDR->sceneColor (including the skybox) for refraction.
        renderer.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
        renderSkyPass(frameInfo, state);
        renderer.endOffscreenRenderPass(frameInfo.commandBuffer);

        // Copy the HDR color buffer (now containing skybox) for transmission refraction sampling + mip chain.
        renderer.copyOffscreenColorToSceneColor(frameInfo.commandBuffer);

        // 3b) Forward overlays
        renderer.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
        state.modelRenderSystem.renderTransmission(frameInfo);
        state.modelRenderSystem.renderAlphaBlend(frameInfo);

        // Dust pass (uses scene lighting conditions)
        auto sunColor     = glm::vec3(1.0f);
        auto ambientColor = glm::vec3(0.1f);

        SunInfo const sunInfo = queryPrimaryDirectionalLightSunInfo(*frameInfo.scene);
        float const   height  = sunInfo.directionToSun.y;
        if (height > 0.1f)
        {
          sunColor     = glm::vec3(1.0f, 0.95f, 0.9f);
          ambientColor = glm::vec3(0.2f, 0.2f, 0.3f);
        }
        else if (height > -0.1f)
        {
          sunColor     = glm::vec3(1.0f, 0.6f, 0.3f);
          ambientColor = glm::vec3(0.3f, 0.2f, 0.2f);
        }
        else
        {
          sunColor     = glm::vec3(0.05f, 0.05f, 0.1f);
          ambientColor = glm::vec3(0.01f, 0.01f, 0.02f);
        }

        glm::vec4 const sunDirWithIntensity = glm::vec4(sunInfo.directionToSun, sunInfo.intensity);
        state.dustRenderSystem.render(frameInfo, state.dustSettings, sunDirWithIntensity, sunColor, ambientColor);

        renderDebugPass(frameInfo, state);
        renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
      }

      frameInfo.globalDescriptorSet = prevGlobalSet;

      renderer.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }));

    // 7. Composition Pass (PostProcess + UI)
    renderGraph->addPass(std::make_unique<LambdaRenderPass>("Composition", [&](FrameInfo& frameInfo) {
      auto state = makeGameLoopState();

      renderer.beginSwapChainRenderPass(frameInfo.commandBuffer);

      auto imageInfo = renderer.getOffscreenImageInfo(frameInfo.frameIndex);
      auto depthInfo = renderer.getDepthImageInfo(frameInfo.frameIndex);
      DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).writeImage(1, &depthInfo).overwrite(postProcessDescriptorSets[frameInfo.frameIndex]);

      postProcessPush.inverseProjection = glm::inverse(camera->getProjection());
      postProcessPush.projection        = camera->getProjection();

      // God Rays Setup
      if (fogSettings.enableGodRays)
      {
        SunInfo const   sunInfo = queryPrimaryDirectionalLightSunInfo(scene);
        glm::vec3 const sunDir  = sunInfo.directionToSun;

        glm::vec3 const sunWorldPos = camera->getPosition() + sunDir * 1000.0f;
        glm::vec4 const clipPos     = camera->getProjection() * camera->getView() * glm::vec4(sunWorldPos, 1.0f);

        if (sunInfo.valid && (sunInfo.intensity > 0.0f) && (clipPos.w > 0.0f))
        {
          glm::vec3 const ndc          = glm::vec3(clipPos) / clipPos.w;
          glm::vec2 const screenPos    = glm::vec2(ndc.x, ndc.y) * 0.5f + 0.5f;
          postProcessPush.sunScreenPos = glm::vec4(screenPos, 1.0f, 0.0f);
        }
        else
        {
          postProcessPush.sunScreenPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        // Dynamic Time-of-Day Adjustment (Golden Hour Boost)
        float const sunHeight           = sunInfo.directionToSun.y;
        float       intensityMultiplier = 1.0f;
        float       decayModifier       = 0.0f;

        // Boost when sun is low (Height -0.1 to 0.5)
        if (sunHeight > -0.1f && sunHeight < 0.5f)
        {
          // Peak effect at height 0.1 (just above horizon)
          float const dist  = glm::abs(sunHeight - 0.1f);
          float const boost = glm::max(0.0f, 1.0f - (dist / 0.4f)); // 0.0 to 1.0

          intensityMultiplier = 1.0f + (boost * 2.0f); // Up to 3x intensity
          decayModifier       = boost * 0.015f;        // Slightly increase decay for longer rays
        }

        postProcessPush.godRayDensity  = fogSettings.godRayDensity;
        postProcessPush.godRayWeight   = fogSettings.godRayWeight * intensityMultiplier;
        postProcessPush.godRayDecay    = glm::clamp(fogSettings.godRayDecay + decayModifier, 0.0f, 0.995f);
        postProcessPush.godRayExposure = fogSettings.godRayExposure * intensityMultiplier;
      }
      else
      {
        postProcessPush.sunScreenPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
      }

      postProcessingSystem->render(frameInfo, postProcessDescriptorSets[frameInfo.frameIndex], postProcessPush);

      uiPhase(frameInfo, frameInfo.commandBuffer, state);
      engine::Renderer::endSwapChainRenderPass(frameInfo.commandBuffer);
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

  void App::update(float /*frameTime*/)
  {
    if (auto* scenePanel = uiManager->getPanel<ScenePanel>())
    {
      scenePanel->processDelayedDeletions(selectedEntity, selectedObjectId);
    }

    // On-demand environment: only load skybox + generate IBL when the user enables skybox display.
    if (showSkybox && (skybox == nullptr))
    {
      std::cout << "[App] Loading skybox..." << '\n';
      skybox = Skybox::loadFromFolder(device, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");

      // Preferred path: load prebaked IBL (offline-generated) instead of regenerating at runtime.
      // Convention: assets/textures/ibl/<SkyboxName>/
      //   irradiance.vtex, prefilter.vtex, brdf_lut.vtex
      if (!iblSystem->loadFromDisk(std::string(TEXTURE_PATH) + "/ibl/Yokohama"))
      {
        std::cout << "[App] No prebaked IBL found for Yokohama (assets/textures/ibl/Yokohama). Using fallback until you regenerate/bake." << '\n';
      }
    }

    // If the user turns off the skybox, also drop IBL back to fallback.
    // Otherwise the last bound IBL descriptor set will keep being sampled.
    if (!showSkybox && (skybox != nullptr))
    {
      std::cout << "[App] Skybox disabled. Resetting IBL to fallback." << '\n';
      skybox.reset();
      iblSystem->resetToFallback();
    }

    iblSystem->update();

    // If IBL images/samplers changed (fallback -> generated, or regeneration), refresh descriptor sets.
    uint64_t const newGen = iblSystem->getGenerationCounter();
    if (newGen != iblGenerationCounter)
    {
      auto irradianceInfo = iblSystem->getIrradianceDescriptorInfo();
      auto prefilterInfo  = iblSystem->getPrefilteredDescriptorInfo();
      auto brdfInfo       = iblSystem->getBRDFLUTDescriptorInfo();

      for (auto& deferredIblDescriptorSet : deferredIblDescriptorSets)
      {
        DescriptorWriter(*deferredIblSetLayout, *deferredIblPool).writeImage(0, &irradianceInfo).writeImage(1, &prefilterInfo).writeImage(2, &brdfInfo).overwrite(deferredIblDescriptorSet);
      }

      iblGenerationCounter = newGen;
    }
  }

  void App::render(float frameTime)
  {
    if (auto commandBuffer = renderer.beginFrame())
    {
      if (renderer.wasSwapChainRecreated())
      {
        postProcessingSystem = std::make_unique<PostProcessingSystem>(device, renderer.getSwapChainRenderPass(), std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});
      }

      int const frameIndex = renderer.getFrameIndex();

      int const                   prevFrameIndex = (frameIndex - 1 + SwapChain::maxFramesInFlight()) % SwapChain::maxFramesInFlight();
      VkDescriptorImageInfo const hzbInfo        = renderer.getHzbImageInfo(prevFrameIndex);
      renderContext->updateHZBDescriptorPrev(frameIndex, hzbInfo);

      // Also pre-bind the descriptor that points to the current frame's HZB image view.
      // It is safe to reference the view before the image is written, as long as the pass ordering/barriers ensure
      // it is only sampled after generation.
      VkDescriptorImageInfo const hzbInfoCurrent = renderer.getHzbImageInfo(frameIndex);
      renderContext->updateHZBDescriptorCurrent(frameIndex, hzbInfoCurrent);

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
              .debugMode           = debugMode,
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

    state.objectSelectionSystem.update(frameInfo);                      // Handle object selection with mouse
    state.inputSystem.update(frameInfo);                                // Process keyboard/mouse input
    engine::LODSystem::update(frameInfo);                               // Update Level of Detail
    engine::CameraSystem::update(frameInfo, renderer.getAspectRatio()); // Update camera matrices
  }

  void App::computePhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update all animations (BEFORE render pass)
    // - Updates AnimationControllers (interpolates morph weights, skeletal
    // transforms)
    // - Dispatches compute shaders for morph targets: baseVertices + deltas *
    // weights → blended
    state.animationSystem.update(frameInfo);
  }

  void App::shadowPhase(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Update uniform buffer with per-frame data FIRST (this also rotates point
    // lights)
    GlobalUbo ubo{};

    // Keep target-locked directional/spot lights oriented correctly for this frame.
    LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

    // Upload dynamic light arrays (SSBO) and reflect counts into the UBO.
    auto const lightCounts    = renderContext->updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);
    ubo.pointLightCount       = lightCounts.point;
    ubo.directionalLightCount = lightCounts.directional;
    ubo.spotLightCount        = lightCounts.spot;

    // Render shadow maps for all shadow-casting lights (mesh shader culling - Level 3)
    state.shadowSystem.renderShadowMaps(frameInfo, 50.0f);

    ubo.projection                  = frameInfo.camera.getProjection();
    ubo.view                        = frameInfo.camera.getView();
    ubo.cameraPosition              = glm::vec4(frameInfo.scene->getRegistry().get<TransformComponent>(frameInfo.cameraEntity).translation, 1.0f);
    ubo.shadowLightCount            = state.shadowSystem.getShadowLightCount();
    ubo.directionalCascadeCount     = state.shadowSystem.getDirectionalCascadeCount();
    ubo.directionalCascadeBaseIndex = state.shadowSystem.getDirectionalCascadeBaseIndex();
    ubo.directionalCascadeSplits    = state.shadowSystem.getDirectionalCascadeSplits();
    ubo.debugMode                   = debugMode;

    // Fog Logic
    glm::vec3 horizonColor   = fogSettings.color;
    glm::vec3 zenithColor    = fogSettings.color;
    float     currentDensity = fogSettings.density;

    if (fogSettings.useSkyColor)
    {
      SunInfo const sunInfo         = queryPrimaryDirectionalLightSunInfo(*frameInfo.scene);
      float const   visualSunHeight = sunInfo.directionToSun.y;

      glm::vec3 const dayHorizon = glm::vec3(0.7f, 0.8f, 0.9f);
      glm::vec3 const dayZenith  = glm::vec3(0.2f, 0.4f, 0.8f);

      glm::vec3 const sunsetHorizon = glm::vec3(0.8f, 0.4f, 0.1f);
      glm::vec3 const sunsetZenith  = glm::vec3(0.2f, 0.2f, 0.4f);

      // Darker night colors to match the starry sky and prevent "glowing fog"
      glm::vec3 const nightHorizon = glm::vec3(0.01f, 0.01f, 0.02f);
      glm::vec3 const nightZenith  = glm::vec3(0.0f, 0.0f, 0.005f);

      if (visualSunHeight > 0.2f)
      {
        horizonColor = dayHorizon;
        zenithColor  = dayZenith;
      }
      else if (visualSunHeight > -0.1f)
      {
        float const t = (visualSunHeight + 0.1f) / 0.3f;
        horizonColor  = glm::mix(sunsetHorizon, dayHorizon, t);
        zenithColor   = glm::mix(sunsetZenith, dayZenith, t);
      }
      else if (visualSunHeight > -0.3f)
      {
        float const t = (visualSunHeight + 0.3f) / 0.2f;
        horizonColor  = glm::mix(nightHorizon, sunsetHorizon, t);
        zenithColor   = glm::mix(nightZenith, sunsetZenith, t);

        // Reduce density at night (fade from 100% to 20%)
        currentDensity = fogSettings.density * glm::mix(0.2f, 1.0f, t);
      }
      else
      {
        horizonColor   = nightHorizon;
        zenithColor    = nightZenith;
        currentDensity = fogSettings.density * 0.2f; // 20% density at night
      }
    }

    ubo.fogColor         = glm::vec4(horizonColor, currentDensity);
    ubo.fogZenithColor   = glm::vec4(zenithColor, 0.0f);
    ubo.fogHeight        = fogSettings.height;
    ubo.fogHeightDensity = fogSettings.heightDensity;

    // HZB Occlusion Culling Settings
    ubo.hzbMaxMipLevel     = hzbSettings.maxMipLevel;
    ubo.hzbMinScreenPixels = hzbSettings.minScreenPixels;
    ubo.hzbScreenSizeScale = hzbSettings.screenSizeScale;
    ubo.hzbEnabled         = hzbSettings.enabled;

    // Calculate Frustum Planes for Culling (Normalized)
    glm::mat4 const vp   = ubo.projection * ubo.view;
    glm::mat4       vpT  = glm::transpose(vp);
    glm::vec4 const row0 = vpT[0];
    glm::vec4 const row1 = vpT[1];
    glm::vec4 const row2 = vpT[2];
    glm::vec4 const row3 = vpT[3];

    ubo.frustumPlanes[0] = row3 + row0; // Left
    ubo.frustumPlanes[1] = row3 - row0; // Right
    ubo.frustumPlanes[2] = row3 + row1; // Bottom
    ubo.frustumPlanes[3] = row3 - row1; // Top
    ubo.frustumPlanes[4] = row2;        // Near
    ubo.frustumPlanes[5] = row3 - row2; // Far

    for (auto& frustumPlane : ubo.frustumPlanes)
    {
      float const len = glm::length(glm::vec3(frustumPlane));
      frustumPlane /= len;
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
    renderSkyPass(frameInfo, state);
    renderGeometryPass(frameInfo, state);
    renderDebugPass(frameInfo, state);
  }

  void App::renderSkyPass(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Skybox currently renders inside the offscreen render pass.
    // This is intentionally isolated so we can later move it after the opaque pass.
    if (state.skybox != nullptr)
    {
      state.skyboxRenderSystem.render(frameInfo, state.skybox, state.skySettings);
    }
  }

  void App::renderGeometryPass(FrameInfo& frameInfo, GameLoopState& state)
  {
    // Legacy helper; the Offscreen render-graph pass now drives the multi-pass mesh pipeline directly.
    (void)frameInfo;
    (void)state;
  }

  void App::renderDebugPass(FrameInfo& frameInfo, GameLoopState& state)
  {
    if (state.showGrid)
    {
      state.gridRenderSystem.render(frameInfo);
    }
    state.lightSystem.render(frameInfo);  // Draw light debug visualizations
    state.cameraSystem.render(frameInfo); // Draw camera debug visualizations
  }

  void App::uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state)
  {
    state.uiManager.render(frameInfo, commandBuffer, window.isCursorVisible());
  }

} // namespace engine