#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/RenderPipeline.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

// Forward declarations
class AnimationSystem;
class CameraSystem;
class InputSystem;
class ObjectSelectionSystem;
class ModelRenderSystem;
class LightSystem;
class RenderContext;
class ShadowSystem;
class LODSystem;
class UIManager;
class Camera;

class IBLSystem;
class ImGuiManager;
class RenderGraph;
class GridRenderSystem;

struct GameLoopState {
  ObjectSelectionSystem& objectSelectionSystem;
  InputSystem& inputSystem;
  CameraSystem& cameraSystem;
  AnimationSystem& animationSystem;
  LODSystem& lodSystem;
  ModelRenderSystem& modelRenderSystem;
  LightSystem& lightSystem;
  ShadowSystem& shadowSystem;
  SkyboxRenderSystem& skyboxRenderSystem;
  GridRenderSystem& gridRenderSystem;
  DustRenderSystem& dustRenderSystem;
  RenderContext& renderContext;
  UIManager& uiManager;
  Skybox* skybox;
  bool showGrid;
  SkyboxSettings& skySettings;
  DustSettings& dustSettings;
  ShadowSettings& shadowSettings;
};

class App {
 public:
  static int width() {
    return 800;
  }
  static int height() {
    return 600;
  }

  App(bool fullscreen = false);
  ~App();

  // delete copy operations
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  void run();

 private:
  void init();
  void setupSystems();
  void setupScene();
  void setupUI();
  void setupRenderGraph();

  void update(float frameTime);
  void render(float frameTime);

  Window window;
  Device device{window};
  Renderer renderer{window, device};
  ResourceManager resourceManager{device};
  SceneSerializer sceneSerializer;  // will be initialized in ctor
  int debugMode = 0;

  // Central engine state (owns systems, scene, resources used by passes)
  EngineState engineState;

  // Input & Camera
  std::unique_ptr<Camera> camera;

  // Demo control: multithreaded secondary-command-buffer recording (opt-in pilot).
  // Uses thread-local command pools and secondary command buffers for G-buffer recording.
  bool multithreadedRecordingEnabled = true;
  uint32_t multithreadedRecordingThreads = 0;

  uint64_t iblGenerationCounter = 0;

  // Render Graph
  std::unique_ptr<RenderPipeline> renderPipeline;

  uint32_t selectedObjectId = 0;

  bool pendingUpdateCameraAfterSceneLoad = false;
};
}  // namespace engine

#endif  // EDITOR_APP_HPP
