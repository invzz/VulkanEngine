#include "Engine/EngineFacade.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

    EngineFacade::EngineFacade(EngineState& state)
        : state_(state) {}

    // --- Systems ---

    ModelRenderSystem& EngineFacade::modelRender() const {
        return *state_.modelRenderSystem;
    }
    ShadowSystem& EngineFacade::shadow() const {
        return *state_.shadowSystem;
    }
    LightSystem& EngineFacade::light() const {
        return *state_.lightSystem;
    }
    SkyboxRenderSystem& EngineFacade::skyboxRender() const {
        return *state_.skyboxRenderSystem;
    }
    GridRenderSystem& EngineFacade::gridRender() const {
        return *state_.gridRenderSystem;
    }
    DeferredLightingSystem& EngineFacade::deferredLighting() const {
        return *state_.deferredLightingSystem;
    }
    PostProcessingSystem& EngineFacade::postProcessing() const {
        return *state_.postProcessingSystem;
    }
    IBLSystem& EngineFacade::ibl() const {
        return *state_.iblSystem;
    }
    CameraSystem& EngineFacade::camera() const {
        return *state_.cameraSystem;
    }
    ColliderDebugRenderSystem& EngineFacade::colliderDebug() const {
        return *state_.colliderDebugRenderSystem;
    }
    MorphTargetManager& EngineFacade::morphTarget() const {
        return *state_.morphTargetManager;
    }
    AnimationSystem& EngineFacade::animation() const {
        return *state_.animationSystem;
    }
    PhysicsSystem& EngineFacade::physics() const {
        return *state_.physicsSystem;
    }
    JoltPhysicsSystem* EngineFacade::joltPhysics() const {
        return state_.joltPhysicsSystem.get();
    }
    InputSystem* EngineFacade::input() const {
        return state_.inputSystem.get();
    }
    ObjectSelectionSystem* EngineFacade::objectSelection() const {
        return state_.objectSelectionSystem.get();
    }

    // --- Scene ---

    Scene& EngineFacade::scene() const {
        return state_.scene;
    }
    entt::entity EngineFacade::selectedEntity() const {
        return state_.selectedEntity;
    }
    entt::entity EngineFacade::cameraEntity() const {
        return state_.cameraEntity;
    }
    void EngineFacade::setSelectedEntity(entt::entity e) {
        state_.selectedEntity = e;
    }
    void EngineFacade::setCameraEntity(entt::entity e) {
        state_.cameraEntity = e;
    }

    // --- Settings ---

    bool& EngineFacade::showSkybox() const {
        return state_.showSkybox;
    }
    bool& EngineFacade::showGrid() const {
        return state_.showGrid;
    }
    bool& EngineFacade::showDebugObjects() const {
        return state_.showDebugObjects;
    }
    bool& EngineFacade::showColliderWireframes() const {
        return state_.showColliderWireframes;
    }
    bool& EngineFacade::debugMode() const {
        return state_.debugMode;
    }
    bool& EngineFacade::physicsSimulationRunning() const {
        return state_.physicsSimulationRunning;
    }
    bool& EngineFacade::solidGroundEnabled() const {
        return state_.solidGroundEnabled;
    }
    SkyboxSettings& EngineFacade::skySettings() const {
        return state_.skySettings;
    }
    ShadowSettings& EngineFacade::shadowSettings() const {
        return state_.shadowSettings;
    }
    PostProcessPushConstants& EngineFacade::postProcessPush() const {
        return state_.postProcessPush;
    }

    // --- Vulkan resources ---

    VkDescriptorSet EngineFacade::gbufferDescriptorSet(int frameIndex) const {
        return state_.getGbufferDescriptorSet(frameIndex);
    }
    VkDescriptorSet EngineFacade::deferredShadowDescriptorSet(int frameIndex) const {
        return state_.getDeferredShadowDescriptorSet(frameIndex);
    }
    VkDescriptorSet EngineFacade::deferredIblDescriptorSet(int frameIndex) const {
        return state_.getDeferredIblDescriptorSet(frameIndex);
    }
    VkDescriptorSet EngineFacade::postProcessDescriptorSet(int frameIndex) const {
        return state_.getPostProcessDescriptorSet(frameIndex);
    }
    DescriptorSetLayout& EngineFacade::gbufferSetLayout() const {
        return state_.gbufferSetLayoutRef();
    }
    DescriptorPool& EngineFacade::gbufferPool() const {
        return state_.gbufferPoolRef();
    }
    DescriptorSetLayout& EngineFacade::postProcessSetLayout() const {
        return state_.postProcessSetLayoutRef();
    }
    DescriptorPool& EngineFacade::postProcessPool() const {
        return state_.postProcessPoolRef();
    }
    DescriptorSetLayout& EngineFacade::deferredIblSetLayout() const {
        return state_.deferredIblSetLayoutRef();
    }
    DescriptorPool& EngineFacade::deferredIblPool() const {
        return state_.deferredIblPoolRef();
    }
    DescriptorSetLayout& EngineFacade::deferredShadowSetLayout() const {
        return state_.deferredShadowSetLayoutRef();
    }
    DescriptorPool& EngineFacade::deferredShadowPool() const {
        return state_.deferredShadowPoolRef();
    }

    // --- Non-owned deps ---

    class IRenderContextPort& EngineFacade::renderContext() const {
        return *state_.renderContextPort;
    }
    class ResourceManager& EngineFacade::resourceManager() const {
        return *state_.resourceManager;
    }

    // --- Post-processing recreation ---

    void EngineFacade::recreatePostProcessingSystem(VkDescriptorSetLayout existingLayout) {
        state_.setPostProcessingSystem(nullptr);
        // Note: The caller must call initPostProcessing again via EngineState.
        // This is a convenience stub — the actual recreation logic stays in EngineState.
    }

    // --- Raw access ---

    EngineState& EngineFacade::raw() const {
        return state_;
    }

}  // namespace engine
