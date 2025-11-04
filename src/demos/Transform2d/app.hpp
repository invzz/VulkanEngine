#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "Device.hpp"
#include "GameObject.hpp"
#include "Model.hpp"
#include "Pipeline.hpp"
#include "Renderer.hpp"
#include "SwapChain.hpp"
#include "Window.hpp"

namespace engine {

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
        void                      loadGameObjects();
        void                      createPipeline();
        void                      createPipelineLayout();
        void                      renderGameObjects(VkCommandBuffer commandBuffer);
        Window                    window{width(), height(), "Engine App"};
        Device                    device{window};
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout          pipelineLayout;
        std::vector<GameObject>   gameObjects;
        Renderer                  renderer{window, device};
    };
} // namespace engine