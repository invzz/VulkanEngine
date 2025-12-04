#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "3dEngine/AnimationController.hpp"
#include "3dEngine/Descriptors.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Renderer.hpp"
#include "3dEngine/ResourceManager.hpp"
#include "3dEngine/SceneSerializer.hpp"
#include "3dEngine/Skybox.hpp"
#include "3dEngine/SwapChain.hpp"
#include "3dEngine/Window.hpp"

namespace engine {

  // Forward declarations
  class AnimationSystem;
  class CameraSystem;
  class InputSystem;
  class ObjectSelectionSystem;
  class MeshRenderSystem;
  class LightSystem;
  class RenderContext;
  class ShadowSystem;
  class SkyboxRenderSystem;
  class LODSystem;
  class UIManager;
  class Camera;

  struct GameLoopState
  {
    ObjectSelectionSystem& objectSelectionSystem;
    InputSystem&           inputSystem;
    CameraSystem&          cameraSystem;
    AnimationSystem&       animationSystem;
    LODSystem&             lodSystem;
    MeshRenderSystem&      meshRenderSystem;
    LightSystem&           lightSystem;
    ShadowSystem&          shadowSystem;
    SkyboxRenderSystem&    skyboxRenderSystem;
    RenderContext&         renderContext;
    UIManager&             uiManager;
    Skybox*                skybox;
  };

  class App
  {
  public:
    static int width() { return 800; }
    static int height() { return 600; }

    App();
    ~App();

    // delete copy operations
    App(const App&)            = delete;
    App& operator=(const App&) = delete;

    void run();

  private:
    void              updatePhase(FrameInfo& frameInfo, GameLoopState& state);
    void              computePhase(FrameInfo& frameInfo, GameLoopState& state);
    void              shadowPhase(FrameInfo& frameInfo, GameLoopState& state);
    void              renderScenePhase(FrameInfo& frameInfo, GameLoopState& state);
    void              uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state);
    Window            window{width(), height(), "Engine App"};
    Device            device{window};
    Renderer          renderer{window, device};
    ResourceManager   resourceManager{device};
    GameObjectManager objectManager;
    SceneSerializer   sceneSerializer{objectManager, resourceManager};
  };
} // namespace engine