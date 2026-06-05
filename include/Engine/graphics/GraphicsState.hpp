#ifndef ENGINE_GRAPHICSSTATE_HPP
#define ENGINE_GRAPHICSSTATE_HPP

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/DescriptorManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
#include "Engine/Scene/Skybox.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

class Device;
class Renderer;
class IRenderContextPort;
class SystemRegistry;

/**
 * @brief Owns all rendering systems, descriptor resources, and scene rendering state.
 *
 * Extracted from EngineState to reduce the God Object anti-pattern.
 * Manages the lifecycle of modelRenderSystem, shadowSystem, lightSystem,
 * skyboxRenderSystem, gridRenderSystem, deferredLightingSystem,
 * postProcessingSystem, iblSystem, and their associated descriptor pools/layouts/sets.
 */
class GraphicsState {
public:
    GraphicsState() = default;
    ~GraphicsState() = default;
    GraphicsState(const GraphicsState&) = delete;
    GraphicsState& operator=(const GraphicsState&) = delete;

    // --- Initialization ---

    /**
     * @brief Initialize all rendering systems and descriptor resources.
     * @param device Vulkan device
     * @param renderer Renderer for render pass and image info queries
     * @param resourceManager Resource manager for texture loading
     * @param renderContextPort Non-owned render context port
     * @param multithreadedRecordingEnabled Whether to enable multithreaded recording
     * @param multithreadedRecordingThreads Number of recording threads
     * @return true if initialization succeeded
     */
    bool initialize(Device& device,
                    Renderer& renderer,
                    ResourceManager& resourceManager,
                    IRenderContextPort* renderContextPort,
                    bool multithreadedRecordingEnabled,
                    uint32_t multithreadedRecordingThreads);

    // --- Accessors — Systems ---

    [[nodiscard]] ModelRenderSystem* modelRenderSystem() const { return modelRenderSystem_.get(); }
    [[nodiscard]] ShadowSystem* shadowSystem() const { return shadowSystem_.get(); }
    [[nodiscard]] LightSystem* lightSystem() const { return lightSystem_.get(); }
    [[nodiscard]] SkyboxRenderSystem* skyboxRenderSystem() const { return skyboxRenderSystem_.get(); }
    [[nodiscard]] GridRenderSystem* gridRenderSystem() const { return gridRenderSystem_.get(); }
    [[nodiscard]] DeferredLightingSystem* deferredLightingSystem() const { return deferredLightingSystem_.get(); }
    [[nodiscard]] PostProcessingSystem* postProcessingSystem() const { return postProcessingSystem_.get(); }
    [[nodiscard]] IBLSystem* iblSystem() const { return iblSystem_.get(); }
    [[nodiscard]] PhysicsSystem* physicsSystem() const { return physicsSystem_.get(); }
    [[nodiscard]] JoltPhysicsSystem* joltPhysicsSystem() const { return joltPhysicsSystem_.get(); }
    [[nodiscard]] AnimationSystem* animationSystem() const { return animationSystem_.get(); }
    [[nodiscard]] ColliderDebugRenderSystem* colliderDebugRenderSystem() const { return colliderDebugRenderSystem_.get(); }
    [[nodiscard]] MorphTargetManager* morphTargetManager() const { return morphTargetManager_.get(); }

    // --- Accessors — Descriptor Manager ---

    [[nodiscard]] DescriptorManager& descriptorManager() const { return *descriptorManager_; }

    // --- Accessors — Scene ---

    [[nodiscard]] Scene& scene() const { return scene_; }
    [[nodiscard]] Scene* scenePtr() { return &scene_; }
    [[nodiscard]] entt::entity& selectedEntity() { return selectedEntity_; }
    [[nodiscard]] entt::entity& cameraEntity() { return cameraEntity_; }
    [[nodiscard]] Skybox* skybox() const { return skybox_.get(); }
    [[nodiscard]] std::unique_ptr<Skybox>& skyboxRef() { return skybox_; }
    [[nodiscard]] SkyboxSettings& skySettings() const { return skySettings_; }
    [[nodiscard]] ShadowSettings& shadowSettings() const { return shadowSettings_; }
    [[nodiscard]] PostProcessPushConstants& postProcessPush() const { return postProcessPush_; }

    // --- Accessors — Runtime Settings ---

    [[nodiscard]] bool showSkybox() const { return showSkybox_; }
    [[nodiscard]] bool& showSkyboxRef() { return showSkybox_; }
    [[nodiscard]] bool showGrid() const { return showGrid_; }
    [[nodiscard]] bool& showGridRef() { return showGrid_; }
    [[nodiscard]] bool showDebugObjects() const { return showDebugObjects_; }
    [[nodiscard]] bool& showDebugObjectsRef() { return showDebugObjects_; }
    [[nodiscard]] bool showColliderWireframes() const { return showColliderWireframes_; }
    [[nodiscard]] bool& showColliderWireframesRef() { return showColliderWireframes_; }
    [[nodiscard]] bool debugMode() const { return debugMode_; }
    [[nodiscard]] bool& debugModeRef() { return debugMode_; }
    [[nodiscard]] bool physicsSimulationRunning() const { return physicsSimulationRunning_; }
    [[nodiscard]] bool& physicsSimulationRunningRef() { return physicsSimulationRunning_; }
    [[nodiscard]] bool solidGroundEnabled() const { return solidGroundEnabled_; }
    [[nodiscard]] bool& solidGroundEnabledRef() { return solidGroundEnabled_; }

    // --- Accessors — Non-owned Dependencies ---

    [[nodiscard]] ResourceManager* resourceManager() const { return resourceManager_; }
    [[nodiscard]] IRenderContextPort* renderContextPort() const { return renderContextPort_; }

    // --- System Registration ---

    /**
     * @brief Register core systems with the system registry.
     * @param registry System registry to register systems with
     * @return true if registration succeeded
     */
    bool registerCoreSystems(SystemRegistry& registry);

    /**
     * @brief Register descriptor resources with the system registry.
     * @param registry System registry to register systems with
     * @return true if registration succeeded
     */
    bool registerDescriptorResources(SystemRegistry& registry);

    /**
     * @brief Register per-frame descriptors with the system registry.
     * @param registry System registry to register systems with
     * @return true if registration succeeded
     */
    bool registerPerFrameDescriptors(SystemRegistry& registry);

    /**
     * @brief Register pipeline links with the system registry.
     * @param registry System registry to register systems with
     * @return true if registration succeeded
     */
    bool registerPipelineLinks(SystemRegistry& registry);

    /**
     * @brief Register post-processing with the system registry.
     * @param registry System registry to register systems with
     * @return true if registration succeeded
     */
    bool registerPostProcessing(SystemRegistry& registry);

private:
    // Rendering systems
    std::unique_ptr<ModelRenderSystem> modelRenderSystem_;
    std::unique_ptr<ShadowSystem> shadowSystem_;
    std::unique_ptr<LightSystem> lightSystem_;
    std::unique_ptr<SkyboxRenderSystem> skyboxRenderSystem_;
    std::unique_ptr<GridRenderSystem> gridRenderSystem_;
    std::unique_ptr<DeferredLightingSystem> deferredLightingSystem_;
    std::unique_ptr<PostProcessingSystem> postProcessingSystem_;
    std::unique_ptr<IBLSystem> iblSystem_;
    std::unique_ptr<PhysicsSystem> physicsSystem_;
    std::unique_ptr<JoltPhysicsSystem> joltPhysicsSystem_;
    std::unique_ptr<AnimationSystem> animationSystem_;
    std::unique_ptr<ColliderDebugRenderSystem> colliderDebugRenderSystem_;
    std::unique_ptr<MorphTargetManager> morphTargetManager_;

    // Descriptor management
    std::unique_ptr<DescriptorManager> descriptorManager_;

    // Scene & rendering state
    Scene scene_;
    entt::entity selectedEntity_ = entt::null;
    entt::entity cameraEntity_ = entt::null;
    std::unique_ptr<Skybox> skybox_;
    SkyboxSettings skySettings_;
    ShadowSettings shadowSettings_;
    PostProcessPushConstants postProcessPush_{};

    // Runtime settings
    bool showSkybox_ = false;
    bool showGrid_ = false;
    bool showDebugObjects_ = false;
    bool showColliderWireframes_ = false;
    bool debugMode_ = false;
    bool physicsSimulationRunning_ = false;
    bool solidGroundEnabled_ = true;

    // Non-owned dependencies
    IRenderContextPort* renderContextPort_ = nullptr;
    ResourceManager* resourceManager_ = nullptr;

    // Initialization helpers
    bool initDescriptorResources(Device& device, Renderer& renderer);
    bool allocatePerFrameDescriptors(Renderer& renderer);
    bool initPostProcessing(Device& device, Renderer& renderer);
};

}  // namespace engine

#endif  // ENGINE_GRAPHICSSTATE_HPP
