#include "Engine/EngineState.hpp"

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Renderer.hpp"

#include "ModelLib/Resources/TextureManager.hpp"

namespace engine {

    void EngineState::initialize(Device& device,
        Renderer&                        renderer,
        ResourceManager&                 resourceManagerRef,
        Window*                          window,
        bool                             multithreadedRecordingEnabled,
        uint32_t                         multithreadedRecordingThreads) {
        // store non-owned resource pointer
        this->resourceManager = &resourceManagerRef;

        // input devices are optional and can be initialized directly
        createInputDevices(window);

        // Phase 3 kickoff: register and initialize subsystems through explicit dependencies.
        systemRegistry.clear();
        std::string registerError;

        if (!systemRegistry.registerSystem(
                "core.systems",
                {},
                [this, &device, &renderer, multithreadedRecordingEnabled, multithreadedRecordingThreads](std::string& error) {
                    initCoreSystems(device, renderer, multithreadedRecordingEnabled, multithreadedRecordingThreads);
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        if (!systemRegistry.registerSystem(
                "descriptor.resources",
                {"core.systems"},
                [this, &device, &renderer](std::string& error) {
                    initDescriptorResources(device, renderer);
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        if (!systemRegistry.registerSystem(
                "per.frame.descriptors",
                {"descriptor.resources"},
                [this, &renderer](std::string& error) {
                    allocatePerFrameDescriptorSets(renderer);
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        if (!systemRegistry.registerSystem(
                "pipeline.links",
                {"core.systems"},
                [this, &renderer](std::string& error) {
                    modelRenderSystem->createDepthPrepassPipeline(renderer.getOffscreenDepthPrepassRenderPass());
                    modelRenderSystem->setShadowSystem(shadowSystem.get());
                    modelRenderSystem->setIBLSystem(iblSystem.get());
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        if (!systemRegistry.registerSystem(
                "post.processing",
                {"per.frame.descriptors", "pipeline.links"},
                [this, &device, &renderer](std::string& error) {
                    initPostProcessing(device, renderer);
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        if (!systemRegistry.registerSystem(
                "input.systems",
                {"core.systems"},
                [this, window](std::string& error) {
                    initInputRelatedSystems(window);
                    return true;
                },
                &registerError)) {
            throw RuntimeException(registerError);
        }

        std::string initError;
        if (!systemRegistry.initializeAll(&initError)) {
            throw RuntimeException(initError);
        }
    }

    // --- helper implementations ---
    void EngineState::createInputDevices(Window* window) {
        if (window != nullptr) {
            keyboard = std::make_unique<Keyboard>(*window);
            mouse    = std::make_unique<Mouse>(*window);
        }
    }

    void EngineState::initCoreSystems(Device& device, Renderer& renderer, bool multithreadedRecordingEnabled, uint32_t multithreadedRecordingThreads) {
        // Camera system depends on render context already being created by caller
        cameraSystem = std::make_unique<CameraSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContext->getGlobalSetLayout());

        // Compute / utility systems
        animationSystem = std::make_unique<AnimationSystem>(device);
        lodSystem       = std::make_unique<LODSystem>();

        // Shadow & IBL
        shadowSystem = std::make_unique<ShadowSystem>(device, 4096);
        iblSystem    = std::make_unique<IBLSystem>(device);

        // Render systems
        skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth());
        gridRenderSystem   = std::make_unique<GridRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContext->getGlobalSetLayout());
        dustRenderSystem   = std::make_unique<DustRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth());
        modelRenderSystem  = std::make_unique<ModelRenderSystem>(device,
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
            auto nInfo              = renderer.getGbufferNormalImageInfo(i);
            auto aInfo              = renderer.getGbufferAlbedoImageInfo(i);
            auto mInfo              = renderer.getGbufferMaterialImageInfo(i);
            auto dInfo              = renderer.getDepthImageInfo(i);
            auto unusedBinding4Info = nInfo;
            auto bakedInfo          = renderer.getGbufferBakedImageInfo(i);
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
            auto prefilterInfo  = iblSystem->getPrefilteredDescriptorInfo();
            auto brdfInfo       = iblSystem->getBRDFLUTDescriptorInfo();
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
        postProcessPool      = DescriptorPool::Builder(device).setMaxSets(SwapChain::maxFramesInFlight()).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 2).build();
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
            inputSystem           = std::make_unique<InputSystem>(*keyboard, *mouse, *window);
        }
    }
}  // namespace engine