#pragma once

#include "entt/entity/fwd.hpp"

namespace engine {

class ModelRenderSystem;
class ShadowSystem;
class LightSystem;
class SkyboxRenderSystem;
class GridRenderSystem;
class DeferredLightingSystem;
class PostProcessingSystem;
class IBLSystem;
class RenderContext;
class Scene;
class Skybox;
class Keyboard;
class Mouse;
class InputSystem;
class ObjectSelectionSystem;
class CameraSystem;
class ResourceManager;
class DescriptorPool;
class DescriptorSetLayout;
class ColliderDebugRenderSystem;
class AnimationSystem;
class LODSystem;
class PhysicsSystem;
class JoltPhysicsSystem;

struct SkyboxSettings;
struct ShadowSettings;

struct RenderingStateView {
  ModelRenderSystem* modelRenderSystem = nullptr;
  ShadowSystem* shadowSystem = nullptr;
  LightSystem* lightSystem = nullptr;
  SkyboxRenderSystem* skyboxRenderSystem = nullptr;
  GridRenderSystem* gridRenderSystem = nullptr;
  DeferredLightingSystem* deferredLightingSystem = nullptr;
  PostProcessingSystem* postProcessingSystem = nullptr;
  IBLSystem* iblSystem = nullptr;
  CameraSystem* camera = nullptr;
  ColliderDebugRenderSystem* colliderDebug = nullptr;
  RenderContext* renderContext = nullptr;
  bool* showSkybox = nullptr;
  bool* showGrid = nullptr;
  bool* showDebugObjects = nullptr;
  bool* showColliderWireframes = nullptr;
  bool* debugMode = nullptr;
  class MorphTargetManager* morphTargetManager = nullptr;
};

struct SceneRuntimeStateView {
  Scene* scene = nullptr;
  entt::entity* selectedEntity = nullptr;
  entt::entity* cameraEntity = nullptr;
  Skybox* skybox = nullptr;
  SkyboxSettings* skySettings = nullptr;
  ShadowSettings* shadowSettings = nullptr;
};

struct InputStateView {
  Keyboard* keyboard = nullptr;
  Mouse* mouse = nullptr;
  InputSystem* inputSystem = nullptr;
  ObjectSelectionSystem* objectSelectionSystem = nullptr;
  CameraSystem* cameraSystem = nullptr;
};

struct ResourceStateView {
  ResourceManager* resourceManager = nullptr;
  RenderContext* renderContext = nullptr;
  DescriptorPool* gbufferPool = nullptr;
  DescriptorSetLayout* gbufferSetLayout = nullptr;
  DescriptorPool* deferredIblPool = nullptr;
  DescriptorSetLayout* deferredIblSetLayout = nullptr;
  DescriptorPool* deferredShadowPool = nullptr;
  DescriptorSetLayout* deferredShadowSetLayout = nullptr;
  DescriptorPool* postProcessPool = nullptr;
  DescriptorSetLayout* postProcessSetLayout = nullptr;
};

struct SystemServicesView {
  ObjectSelectionSystem* objectSelection = nullptr;
  InputSystem* input = nullptr;
  CameraSystem* camera = nullptr;
  ColliderDebugRenderSystem* colliderDebug = nullptr;
  AnimationSystem* animation = nullptr;
  LODSystem* lod = nullptr;
  ModelRenderSystem* modelRender = nullptr;
  ShadowSystem* shadow = nullptr;
  LightSystem* light = nullptr;
  SkyboxRenderSystem* skyboxRender = nullptr;
  GridRenderSystem* gridRender = nullptr;
  DeferredLightingSystem* deferredLighting = nullptr;
  PostProcessingSystem* postProcessing = nullptr;
  IBLSystem* ibl = nullptr;
  PhysicsSystem* physics = nullptr;
  JoltPhysicsSystem* joltPhysics = nullptr;
};

}  // namespace engine