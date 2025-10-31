#pragma once

#include <memory>

#include "Device.hpp"
#include "Model.hpp"
#include "Pipeline.hpp"
#include "SwapChain.hpp"
#include "Window.hpp"

namespace engine {

    class App
    {
      public:
        static constexpr int WIDTH  = 1280;
        static constexpr int HEIGHT = 720;

        App();
        ~App();
        // delete copy operations
        App(const App&)            = delete;
        App& operator=(const App&) = delete;

        void run();

      private:
        void loadModels();
        void createPipeline();
        void createPipelineLayout();
        void createCommandBuffers();
        void drawFrame();

        void recreateSwapChain();
        void recordCommandBuffer(int imageIndex);

        Window                       window{WIDTH, HEIGHT, "Engine App"};
        Device                       device{window};
        std::unique_ptr<SwapChain>   swapChain;
        std::unique_ptr<Pipeline>    pipeline;
        VkPipelineLayout             pipelineLayout;
        std::vector<VkCommandBuffer> commandBuffers;
        std::unique_ptr<Model>       model;
    };

} // namespace engine