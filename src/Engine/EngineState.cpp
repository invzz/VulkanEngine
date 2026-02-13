#include "Engine/EngineState.hpp"

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "ModelLib/Resources/TextureManager.hpp"

namespace engine {

void EngineState::initialize(Device& device,
    Renderer& renderer,
    ResourceManager& resourceManagerRef,
    Window* window,
    bool multithreadedRecordingEnabled,
    uint32_t multithreadedRecordingThreads) {
  // store non-owned resource pointer
  this->resourceManager = &resourceManagerRef;

  // high-level orchestration — helpers keep intent explicit and testable
  createInputDevices(window);
  initCoreSystems(device, renderer, multithreadedRecordingEnabled, multithreadedRecordingThreads);
  initDescriptorResources(device, renderer);
  allocatePerFrameDescriptorSets(renderer);

  // Depth prepass pipeline and system links
  modelRenderSystem->createDepthPrepassPipeline(renderer.getOffscreenDepthPrepassRenderPass());
  modelRenderSystem->setShadowSystem(shadowSystem.get());
  modelRenderSystem->setIBLSystem(iblSystem.get());

  initPostProcessing(device, renderer);
  initInputRelatedSystems(window);
}

// --- helper implementations ---
void EngineState::createInputDevices(Window* window) {
  if (window != nullptr) {
    keyboard = std::make_unique<Keyboard>(*window);
    mouse = std::make_unique<Mouse>(*window);
  }
}

void EngineState::initCoreSystems(Device& device, Renderer& renderer, bool multithreadedRecordingEnabled, uint32_t multithreadedRecordingThreads) {
  // Camera system depends on render context already being created by caller
  cameraSystem = std::make_unique<CameraSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContext->getGlobalSetLayout());

  // Compute / utility systems
  animationSystem = std::make_unique<AnimationSystem>(device);
  lodSystem = std::make_unique<LODSystem>();

  // Shadow & IBL
  shadowSystem = std::make_unique<ShadowSystem>(device, 4096);
  iblSystem = std::make_unique<IBLSystem>(device);

  // Render systems
  skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth());
  gridRenderSystem = std::make_unique<GridRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContext->getGlobalSetLayout());
  dustRenderSystem = std::make_unique<DustRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth());
  modelRenderSystem = std::make_unique<ModelRenderSystem>(device,
      renderer.getOffscreenRenderPassLoadColorDepth(),
      renderContext->getGlobalSetLayout(),
      this->resourceManager->getTextureManager().getDescriptorSetLayout());

  modelRenderSystem->enableMultiThreadedRecording(multithreadedRecordingEnabled, multithreadedRecordingThreads);
  lightSystem = std::make_unique<LightSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContext->getGlobalSetLayout());
}

void EngineState::initDescriptorResources(Device& device, Renderer& renderer) {
  // G-buffer + Deferred lighting
  modelRenderSystem->createGbufferPipeline(renderer.getGbufferRenderPass());

  gbufferPool = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 6).build();

  gbufferSetLayout = DescriptorSetLayout::Builder(device)
                         .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                         .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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

  // Note: Shadow set must be placed before IBL set to match LightingRenderBindings indices
  deferredLightingSystem = std::make_unique<DeferredLightingSystem>(device,
      renderer.getDeferredLightingRenderPass(),
      std::vector<VkDescriptorSetLayout>{renderContext->getGlobalSetLayout(),
          gbufferSetLayout->getDescriptorSetLayout(),
          deferredShadowSetLayout->getDescriptorSetLayout(),
          deferredIblSetLayout->getDescriptorSetLayout()});
}

void EngineState::allocatePerFrameDescriptorSets(Renderer& renderer) {
  // Allocate + build per-frame descriptor sets that reference renderer images
  gbufferDescriptorSets.resize(SwapChain::maxFramesInFlight());
  for (int i = 0; i < static_cast<int>(gbufferDescriptorSets.size()); ++i) {
    auto nInfo = renderer.getGbufferNormalImageInfo(i);
    auto aInfo = renderer.getGbufferAlbedoImageInfo(i);
    auto mInfo = renderer.getGbufferMaterialImageInfo(i);
    auto dInfo = renderer.getDepthImageInfo(i);
    auto unusedBinding4Info = nInfo;
    auto bakedInfo = renderer.getGbufferBakedImageInfo(i);
    DescriptorWriter(*gbufferSetLayout, *gbufferPool)
        .writeImage(0, &nInfo)
        .writeImage(1, &aInfo)
        .writeImage(2, &mInfo)
        .writeImage(3, &dInfo)
        .writeImage(4, &unusedBinding4Info)
        .writeImage(5, &bakedInfo)
        .build(gbufferDescriptorSets[i]);
  }

  deferredIblDescriptorSets.resize(SwapChain::maxFramesInFlight());
  for (int i = 0; i < static_cast<int>(deferredIblDescriptorSets.size()); ++i) {
    auto irradianceInfo = iblSystem->getIrradianceDescriptorInfo();
    auto prefilterInfo = iblSystem->getPrefilteredDescriptorInfo();
    auto brdfInfo = iblSystem->getBRDFLUTDescriptorInfo();
    DescriptorWriter(*deferredIblSetLayout, *deferredIblPool).writeImage(0, &irradianceInfo).writeImage(1, &prefilterInfo).writeImage(2, &brdfInfo).build(deferredIblDescriptorSets[i]);
  }

  deferredShadowDescriptorSets.resize(SwapChain::maxFramesInFlight());
  for (auto& ds : deferredShadowDescriptorSets) {
    if (!deferredShadowPool->allocateDescriptor(deferredShadowSetLayout->getDescriptorSetLayout(), ds)) {
      throw std::runtime_error("Failed to allocate deferred shadow descriptor set");
    }
  }
}

void EngineState::initPostProcessing(Device& device, Renderer& renderer) {
  // Post processing resources
  postProcessPool = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 2).build();
  postProcessSetLayout = DescriptorSetLayout::Builder(device)
                             .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .build();

  postProcessingSystem = std::make_unique<PostProcessingSystem>(device, renderer.getSwapChainRenderPass(), std::vector<VkDescriptorSetLayout>{postProcessSetLayout->getDescriptorSetLayout()});
  postProcessDescriptorSets.resize(SwapChain::maxFramesInFlight());
  for (int i = 0; i < static_cast<int>(postProcessDescriptorSets.size()); ++i) {
    auto imageInfo = renderer.getOffscreenImageInfo(i);
    auto depthInfo = renderer.getDepthImageInfo(i);
    DescriptorWriter(*postProcessSetLayout, *postProcessPool).writeImage(0, &imageInfo).writeImage(1, &depthInfo).build(postProcessDescriptorSets[i]);
  }
}

void EngineState::initInputRelatedSystems(Window* window) {
  // Input-related systems (optional) — EngineState now owns Keyboard/Mouse
  if ((keyboard != nullptr) && (mouse != nullptr) && (window != nullptr)) {
    objectSelectionSystem = std::make_unique<ObjectSelectionSystem>(*keyboard);
    inputSystem = std::make_unique<InputSystem>(*keyboard, *mouse, *window);
  }

}  
}  // namespace engine