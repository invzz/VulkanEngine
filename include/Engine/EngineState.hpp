#pragma once

#include <memory>
#include <vector>

#include "Editor/RenderContext.hpp"
#include "Editor/ui/UIManager.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
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
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

class Device;
class Renderer;
class Keyboard;
class Mouse;
class Window;

// EngineState is the single source-of-truth for owned systems, scene and
// runtime settings. Pass a pointer/reference to render passes and systems
// so they can access the current runtime state without long parameter lists.
class EngineState {
 public:
  // lifecycle
  void initialize(Device& device,
      Renderer& renderer,
      ResourceManager& resourceManager,
      Window* window,
      bool multithreadedRecordingEnabled,
      uint32_t multithreadedRecordingThreads);

 private:
  // initialization helpers - keep initialize() high-level and explicit
  void createInputDevices(Window* window);
  void initCoreSystems(Device& device, Renderer& renderer, bool multithreadedRecordingEnabled, uint32_t multithreadedRecordingThreads);
  void initDescriptorResources(Device& device, Renderer& renderer);
  void allocatePerFrameDescriptorSets(Renderer& renderer);
  void initPostProcessing(Device& device, Renderer& renderer);
  void initInputRelatedSystems(Window* window);

 public:
  // Systems
  std::unique_ptr<ObjectSelectionSystem> objectSelectionSystem;
  std::unique_ptr<InputSystem> inputSystem;
  std::unique_ptr<CameraSystem> cameraSystem;
  std::unique_ptr<AnimationSystem> animationSystem;
  std::unique_ptr<LODSystem> lodSystem;
  std::unique_ptr<ModelRenderSystem> modelRenderSystem;
  std::unique_ptr<ShadowSystem> shadowSystem;
  std::unique_ptr<LightSystem> lightSystem;
  std::unique_ptr<SkyboxRenderSystem> skyboxRenderSystem;
  std::unique_ptr<GridRenderSystem> gridRenderSystem;
  std::unique_ptr<DustRenderSystem> dustRenderSystem;
  std::unique_ptr<DeferredLightingSystem> deferredLightingSystem;
  std::unique_ptr<PostProcessingSystem> postProcessingSystem;
  std::unique_ptr<IBLSystem> iblSystem;

  // Resources
  std::unique_ptr<RenderContext> renderContext;
  ResourceManager* resourceManager = nullptr;  // not owned here

  // Input devices (owned by EngineState)
  std::unique_ptr<Keyboard> keyboard;
  std::unique_ptr<Mouse> mouse;

  // Scene & entities
  Scene scene;
  entt::entity selectedEntity = entt::null;
  entt::entity cameraEntity = entt::null;

  // UI
  std::unique_ptr<UIManager> uiManager;
  std::unique_ptr<ImGuiManager> imguiManager;

  // Descriptor/layout state used by several passes
  std::unique_ptr<DescriptorPool> gbufferPool;
  std::unique_ptr<DescriptorSetLayout> gbufferSetLayout;
  std::vector<VkDescriptorSet> gbufferDescriptorSets;

  std::unique_ptr<DescriptorPool> deferredIblPool;
  std::unique_ptr<DescriptorSetLayout> deferredIblSetLayout;
  std::vector<VkDescriptorSet> deferredIblDescriptorSets;

  std::unique_ptr<DescriptorPool> deferredShadowPool;
  std::unique_ptr<DescriptorSetLayout> deferredShadowSetLayout;
  std::vector<VkDescriptorSet> deferredShadowDescriptorSets;

  std::unique_ptr<DescriptorPool> postProcessPool;
  std::unique_ptr<DescriptorSetLayout> postProcessSetLayout;
  std::vector<VkDescriptorSet> postProcessDescriptorSets;
  PostProcessPushConstants postProcessPush{};

  // Scene resources
  std::unique_ptr<Skybox> skybox;
  SkyboxSettings skySettings;
  DustSettings dustSettings;
  FogSettings fogSettings;
  HZBSettings hzbSettings;
  ShadowSettings shadowSettings;

  // View toggles
  bool showSkybox = false;
  bool showGrid = false;
  bool debugMode = false;
};

}  // namespace engine
