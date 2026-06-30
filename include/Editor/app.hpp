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

#include "Editor/ui/ViewportPanel.hpp"
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

        App(bool fullscreen = false);
        ~App();

        App(const App&)            = delete;
        App& operator=(const App&) = delete;

        void run();

       private:
        void init();
        void setupScene();
        void setupUI();
        void setupRenderGraph();

        void update(float frameTime);
        void render(float frameTime);

        Window          window;
        Device          device{window};
        Renderer        renderer{window, device};
        ResourceManager resourceManager{device};
        int             debugMode = 0;

        EngineState     engineState;
        SceneSerializer sceneSerializer;

        std::unique_ptr<RenderContext>        renderContext;
        std::unique_ptr<RenderContextAdapter> renderContextAdapter;
        std::unique_ptr<ImGuiManager>         imguiManager;
        std::unique_ptr<UIManager>            uiManager;

        std::unique_ptr<Camera> camera;

        // Viewport
        Viewport       viewport_;
        ViewportPanel* viewportPanel_ = nullptr;

        struct {
            bool       pending_ = false;
            VkExtent2D extent_{};
        } viewportResize_;

        bool     multithreadedRecordingEnabled = true;
        uint32_t multithreadedRecordingThreads = 0;

        std::unique_ptr<RenderPipeline> renderPipeline;

        uint32_t selectedObjectId = 0;
    };

}  // namespace engine

#endif  // EDITOR_APP_HPP
