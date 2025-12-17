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
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

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
  class LODSystem;
  class UIManager;
  class Camera;
  class Keyboard;
  class Mouse;
  class IBLSystem;
  class ImGuiManager;
  class RenderGraph;

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
    SkyboxSettings&        skySettings;
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
    void init();
    void setupSystems();
    void setupScene();
    void setupUI();
    void setupRenderGraph();

    void update(float frameTime);
    void render(float frameTime);

    void updatePhase(FrameInfo& frameInfo, GameLoopState& state);
    void computePhase(FrameInfo& frameInfo, GameLoopState& state);
    void shadowPhase(FrameInfo& frameInfo, GameLoopState& state);
    void renderScenePhase(FrameInfo& frameInfo, GameLoopState& state);
    void uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state);

    Window          window{width(), height(), "Engine App"};
    Device          device{window};
    Renderer        renderer{window, device};
    ResourceManager resourceManager{device};
    Scene           scene;
    SceneSerializer sceneSerializer{scene, resourceManager};
    int             debugMode = 0;

    // Core Systems
    std::unique_ptr<RenderContext> renderContext;

    // Input & Camera
    std::unique_ptr<Camera>   camera;
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Mouse>    mouse;
    entt::entity              cameraEntity{entt::null};

    // Game Systems
    std::unique_ptr<ObjectSelectionSystem> objectSelectionSystem;
    std::unique_ptr<InputSystem>           inputSystem;
    std::unique_ptr<CameraSystem>          cameraSystem;
    std::unique_ptr<AnimationSystem>       animationSystem;
    std::unique_ptr<LODSystem>             lodSystem;
    std::unique_ptr<ShadowSystem>          shadowSystem;
    std::unique_ptr<IBLSystem>             iblSystem;

    // Render Systems
    std::unique_ptr<SkyboxRenderSystem>   skyboxRenderSystem;
    std::unique_ptr<MeshRenderSystem>     meshRenderSystem;
    std::unique_ptr<LightSystem>          lightSystem;
    std::unique_ptr<PostProcessingSystem> postProcessingSystem;

    // Scene Resources
    std::unique_ptr<Skybox> skybox;
    SkyboxSettings          skySettings;
    float                   timeOfDay{0.0f};
    float                   daySpeed{0.1f};
    glm::vec4               lastSunDirection{0.0f};

    // UI
    std::unique_ptr<ImGuiManager> imguiManager;
    std::unique_ptr<UIManager>    uiManager;

    // Render Graph
    std::unique_ptr<RenderGraph> renderGraph;

    // State
    std::unique_ptr<DescriptorPool>      postProcessPool;
    std::unique_ptr<DescriptorSetLayout> postProcessSetLayout;
    std::vector<VkDescriptorSet>         postProcessDescriptorSets;
    PostProcessPushConstants             postProcessPush{};

    uint32_t     selectedObjectId = 0;
    entt::entity selectedEntity   = entt::null;
  };
} // namespace engine