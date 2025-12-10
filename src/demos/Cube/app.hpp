#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneSerializer.hpp"
#include "Engine/Scene/Skybox.hpp"

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
    void            updatePhase(FrameInfo& frameInfo, GameLoopState& state);
    void            computePhase(FrameInfo& frameInfo, GameLoopState& state);
    void            shadowPhase(FrameInfo& frameInfo, GameLoopState& state);
    void            renderScenePhase(FrameInfo& frameInfo, GameLoopState& state);
    void            uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state);
    Window          window{width(), height(), "Engine App"};
    Device          device{window};
    Renderer        renderer{window, device};
    ResourceManager resourceManager{device};
    Scene           scene;
    SceneSerializer sceneSerializer{scene, resourceManager};
    int             debugMode = 0;
  };
} // namespace engine