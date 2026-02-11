#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP

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
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
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
  class Keyboard;
  class Mouse;
  class IBLSystem;
  class ImGuiManager;
  class RenderGraph;
  class GridRenderSystem;

  struct GameLoopState
  {
    ObjectSelectionSystem& objectSelectionSystem;
    InputSystem&           inputSystem;
    CameraSystem&          cameraSystem;
    AnimationSystem&       animationSystem;
    LODSystem&             lodSystem;
    ModelRenderSystem&     modelRenderSystem;
    LightSystem&           lightSystem;
    ShadowSystem&          shadowSystem;
    SkyboxRenderSystem&    skyboxRenderSystem;
    GridRenderSystem&      gridRenderSystem;
    DustRenderSystem&      dustRenderSystem;
    RenderContext&         renderContext;
    UIManager&             uiManager;
    Skybox*                skybox;
    bool                   showGrid;
    SkyboxSettings&        skySettings;
    DustSettings&          dustSettings;
    ShadowSettings&        shadowSettings;
  };

  class App
  {
  public:
    static int width() { return 800; }
    static int height() { return 600; }

    App(bool fullscreen = false);
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

    GameLoopState makeGameLoopState();

    void update(float frameTime);
    void render(float frameTime);

    void        updatePhase(FrameInfo& frameInfo, GameLoopState& state);
    static void computePhase(FrameInfo& frameInfo, GameLoopState& state);
    void        shadowPhase(FrameInfo& frameInfo, GameLoopState& state);
    static void renderScenePhase(FrameInfo& frameInfo, GameLoopState& state);
    static void renderSkyPass(FrameInfo& frameInfo, GameLoopState& state);
    static void renderGeometryPass(FrameInfo& frameInfo, GameLoopState& state);
    static void renderDebugPass(FrameInfo& frameInfo, GameLoopState& state);
    void        uiPhase(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, GameLoopState& state);

    Window          window;
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
    std::unique_ptr<SkyboxRenderSystem>     skyboxRenderSystem;
    std::unique_ptr<GridRenderSystem>       gridRenderSystem;
    std::unique_ptr<DustRenderSystem>       dustRenderSystem;
    std::unique_ptr<ModelRenderSystem>      modelRenderSystem;
    std::unique_ptr<LightSystem>            lightSystem;
    std::unique_ptr<DeferredLightingSystem> deferredLightingSystem;
    std::unique_ptr<PostProcessingSystem>   postProcessingSystem;

    // Scene Resources
    std::unique_ptr<Skybox> skybox;
    SkyboxSettings          skySettings;
    DustSettings            dustSettings;
    FogSettings             fogSettings;
    HZBSettings             hzbSettings;
    ShadowSettings          shadowSettings;

    // View toggles
    bool showSkybox = false;
    bool showGrid   = false;

    // Demo control: multithreaded secondary-command-buffer recording (opt-in pilot).
    // Uses thread-local command pools and secondary command buffers for G-buffer recording.
    bool     multithreadedRecordingEnabled = true;
    uint32_t multithreadedRecordingThreads = 0;

    uint64_t iblGenerationCounter = 0;

    // UI
    std::unique_ptr<ImGuiManager> imguiManager;
    std::unique_ptr<UIManager>    uiManager;

    // Render Graph
    std::unique_ptr<RenderGraph> renderGraph;

    // State
    std::unique_ptr<DescriptorPool>      gbufferPool;
    std::unique_ptr<DescriptorSetLayout> gbufferSetLayout;
    std::vector<VkDescriptorSet>         gbufferDescriptorSets;

    std::unique_ptr<DescriptorPool>      deferredIblPool;
    std::unique_ptr<DescriptorSetLayout> deferredIblSetLayout;
    std::vector<VkDescriptorSet>         deferredIblDescriptorSets;

    std::unique_ptr<DescriptorPool>      deferredShadowPool;
    std::unique_ptr<DescriptorSetLayout> deferredShadowSetLayout;
    std::vector<VkDescriptorSet>         deferredShadowDescriptorSets;

    std::unique_ptr<DescriptorPool>      postProcessPool;
    std::unique_ptr<DescriptorSetLayout> postProcessSetLayout;
    std::vector<VkDescriptorSet>         postProcessDescriptorSets;
    PostProcessPushConstants             postProcessPush{};

    uint32_t     selectedObjectId = 0;
    entt::entity selectedEntity   = entt::null;

    bool pendingUpdateCameraAfterSceneLoad = false;
  };
} // namespace engine

#endif // EDITOR_APP_HPP
