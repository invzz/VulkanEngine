#include "Engine/EngineState.hpp"

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/DescriptorManager.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

#include "ModelLib/Resources/TextureManager.hpp"

namespace engine {

    EngineState::~EngineState() = default;

    void EngineState::initialize(Device& device,
        Renderer&                        renderer,
        ResourceManager&                 resourceManagerRef,
        IRenderContextPort*              requiredRenderContextPort,
        Window*                          window,
        bool                             multithreadedRecordingEnabled,
        uint32_t                         multithreadedRecordingThreads) {
        if (requiredRenderContextPort == nullptr) {
            throw RuntimeException("EngineState::initialize requires a non-null IRenderContextPort");
        }

        // store non-owned resource pointer
        this->resourceManager   = &resourceManagerRef;
        this->renderContextPort = requiredRenderContextPort;

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

        if (!systemRegistry.registerSystem(
                "physics.systems",
                {"core.systems"},
                [this, &device](std::string& error) {
                    physicsSystem     = std::make_unique<PhysicsSystem>(device);
                    joltPhysicsSystem = std::make_unique<JoltPhysicsSystem>();
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
        if (renderContextPort == nullptr) {
            throw RuntimeException("EngineState::initCoreSystems called with null IRenderContextPort");
        }

        // Camera system depends on render context already being created by caller
        cameraSystem              = std::make_unique<CameraSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContextPort->getGlobalSetLayout());
        colliderDebugRenderSystem = std::make_unique<ColliderDebugRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContextPort->getGlobalSetLayout());

        // Compute / utility systems
        animationSystem = std::make_unique<AnimationSystem>(device);
        lodSystem       = std::make_unique<LODSystem>();

        // Shadow & IBL
        shadowSystem       = std::make_unique<ShadowSystem>(device, 4096);
        iblSystem          = std::make_unique<IBLSystem>(device);
        morphTargetManager = std::make_unique<MorphTargetManager>(device);

        // Render systems
        skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth());
        gridRenderSystem   = std::make_unique<GridRenderSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContextPort->getGlobalSetLayout());
        modelRenderSystem  = std::make_unique<ModelRenderSystem>(device,
            renderer.getOffscreenRenderPassLoadColorDepth(),
            renderContextPort->getGlobalSetLayout(),
            this->resourceManager->getTextureManager().getDescriptorSetLayout());

        modelRenderSystem->enableMultiThreadedRecording(multithreadedRecordingEnabled, multithreadedRecordingThreads);
        lightSystem = std::make_unique<LightSystem>(device, renderer.getOffscreenRenderPassLoadColorDepth(), renderContextPort->getGlobalSetLayout());
    }

    void EngineState::initDescriptorResources(Device& device, Renderer& renderer) {
        // Create the descriptor manager (owns all pools/layouts/sets).
        descriptorManager = std::make_unique<DescriptorManager>();
        descriptorManager->createDescriptorResources(device, renderer);

        // G-buffer pipeline (still needs modelRenderSystem).
        modelRenderSystem->createGbufferPipeline(renderer.getGbufferRenderPass());

        // Deferred lighting system needs descriptor layouts from the manager.
        deferredLightingSystem = std::make_unique<DeferredLightingSystem>(device,
            renderer.getDeferredLightingRenderPass(),
            std::vector<VkDescriptorSetLayout>{renderContextPort->getGlobalSetLayout(),
                descriptorManager->gbufferSetLayout().getDescriptorSetLayout(),
                descriptorManager->deferredShadowSetLayout().getDescriptorSetLayout(),
                descriptorManager->deferredIblSetLayout().getDescriptorSetLayout()});
    }

    void EngineState::allocatePerFrameDescriptorSets(Renderer& renderer) {
        // Delegate descriptor set allocation to the manager.
        descriptorManager->allocatePerFrameDescriptors(renderer);

        // IBL descriptor sets need IBLSystem access — allocate separately.
        deferredIblDescriptorSetsRef().resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(deferredIblDescriptorSetsRef().size()); ++i) {
            auto irradianceInfo = iblSystem->getIrradianceDescriptorInfo();
            auto prefilterInfo  = iblSystem->getPrefilteredDescriptorInfo();
            auto brdfInfo       = iblSystem->getBRDFLUTDescriptorInfo();
            DescriptorWriter(descriptorManager->deferredIblSetLayout(),
                descriptorManager->deferredIblPool())
                .writeImage(0, &irradianceInfo)
                .writeImage(1, &prefilterInfo)
                .writeImage(2, &brdfInfo)
                .buildOrThrow(deferredIblDescriptorSetsRef()[i]);
        }

        // Validate IBL descriptor sets are not null
        for (const auto& ds : deferredIblDescriptorSetsRef()) {
            assert(ds != VK_NULL_HANDLE && "IBL descriptor set failed to allocate");
        }
    }

    void EngineState::initPostProcessing(Device& device, Renderer& renderer) {
        // Post-processing descriptor resources are now owned by DescriptorManager.
        descriptorManager->recreatePostProcessDescriptorSets(device, renderer, VK_NULL_HANDLE);

        // Post-processing system (layout from manager).
        postProcessingSystem = std::make_unique<PostProcessingSystem>(
            device, renderer.getSwapChainRenderPass(),
            std::vector<VkDescriptorSetLayout>{descriptorManager->postProcessSetLayout().getDescriptorSetLayout()});
    }

    void EngineState::initInputRelatedSystems(Window* window) {
        // Input-related systems (optional) — EngineState now owns Keyboard/Mouse
        if ((keyboard != nullptr) && (mouse != nullptr) && (window != nullptr)) {
            objectSelectionSystem = std::make_unique<ObjectSelectionSystem>(*keyboard);
            inputSystem           = std::make_unique<InputSystem>(*keyboard, *mouse, *window);
        }
    }
}  // namespace engine