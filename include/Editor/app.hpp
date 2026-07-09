#ifndef EDITOR_APP_HPP
#define EDITOR_APP_HPP
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Graphics/RenderPipeline.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/Viewport.hpp"

#include "Engine/Graphics/AccelBuilder.hpp"

#include "Editor/ui/Panels/ViewportPanel.hpp"
#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
namespace engine {
    class UIManager;
    class Camera;
    class CameraSystem;
    class ImGuiManager;
    class RenderContext;
    class App {
       public:
        static int width() {
            return 800;
        }
        static int height() {
            return 600;
        }
        App(bool fullscreen = false, std::string modelPath = {});
        ~App();
        App(const App&)            = delete;
        App& operator=(const App&) = delete;
        void run();

       private:
        void                                  init();
        void                                  setupScene();
        void                                  setupUI();
        void                                  setupRenderGraph();
        void                                  update(float frameTime);
        void                                  render(float frameTime);
        void                                  handleViewportResize();
        void                                  applyViewportPicking(FrameInfo& frameInfo);
        void                                  rebuildAccelerationStructures(VkCommandBuffer commandBuffer);
        void                                  syncStateFromFrame(const FrameInfo& frameInfo);
        FrameInfo                             buildFrameInfo(int frameIndex, float frameTime, VkCommandBuffer commandBuffer);
        
        Window                                window;
        Device                                device{window};
        Renderer                              renderer{window, device};
        ResourceManager                       resourceManager{device};
        int                                   debugMode = 0;
        bool                                  rtDirectional = true;
        bool                                  rtPoint       = true;
        bool                                  rtSpot        = true;
        float                                 rtShadowSoftness = 0.005f;
        EngineState                           engineState;
        SceneSerializer                       sceneSerializer;
        std::unique_ptr<RenderContext>        renderContext;
        std::unique_ptr<RenderContextAdapter> renderContextAdapter;
        std::unique_ptr<ImGuiManager>         imguiManager;
        std::unique_ptr<UIManager>            uiManager;
        std::unique_ptr<Camera>               camera;
        Viewport                              viewport_;
        ViewportPanel*                        viewportPanel_ = nullptr;
        struct {
            bool       pending_ = false;
            VkExtent2D extent_{};
        } viewportResize_;
        bool                            multithreadedRecordingEnabled = true;
        uint32_t                        multithreadedRecordingThreads = 0;
        std::unique_ptr<RenderPipeline> renderPipeline;
        uint32_t                        selectedObjectId = 0;
        // Viewport picking is captured by the UI at the END of a frame (UI renders
        // last), but consumed at the START of the next frame's picking pass. Carry
        // the pending click across frames so the click is not lost.
        bool                            pendingViewportClick_ = false;
        glm::vec2                       pendingViewportMousePos_{};
        std::unique_ptr<AccelBuilder>   accelBuilder;
        using BlasInstance = std::pair<glm::mat4, VkAccelerationStructureKHR>;
        std::vector<BlasInstance>               tlasInstances_;
        // Per-instance submesh opacity data for ray-traced transparent shadows
        std::vector<uint32_t> instanceSubmeshHeaders_;
        std::vector<uint32_t> instanceSubmeshData_;
        /// Optional model path to load at startup instead of scene.json.
        std::string modelPath_;
    };
}  // namespace engine
#endif
