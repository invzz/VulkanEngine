#pragma once

#include <memory>

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
class ColliderDebugRenderSystem;
class MorphTargetManager;
class CameraSystem;

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
  MorphTargetManager* morphTargetManager = nullptr;
};

}  // namespace engine
