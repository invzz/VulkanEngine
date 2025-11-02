#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "Device.hpp"
#include "GameObject.hpp"
#include "Model.hpp"
#include "Pipeline.hpp"
#include "SwapChain.hpp"
#include "Window.hpp"

namespace engine {

    class App
    {
      public:
        static int width() { return 1280; }
        static int height() { return 720; }

        App();
        ~App();
        // delete copy operations
        App(const App&)            = delete;
        App& operator=(const App&) = delete;

        void run();

      private:
        struct AnimationTrack
        {
            int       armIndex{0};
            int       armCount{1};
            float     radius{0.0f};
            float     orbitSpeed{0.0f};
            float     orbitPhase{0.0f};
            float     radialPulseAmplitude{0.0f};
            float     radialPulseFrequency{0.0f};
            glm::vec2 baseScale{1.0f};
            glm::vec2 scalePulseAmplitude{0.0f};
            float     scalePulseFrequency{0.0f};
            float     baseRotation{0.0f};
            float     rotationSpeed{0.0f};
            bool      mirror{false};
            glm::vec3 baseColor{0.0f, 1.0f, 0.0f};
            glm::vec3 colorCycle{0.0f};
            float     colorFrequency{0.0f};
        };

        void loadGameObjects();
        void createPipeline();
        void createPipelineLayout();
        void createCommandBuffers();
        void drawFrame();
        void freeCommandBuffers();
        void recreateSwapChain();
        void recordCommandBuffer(int imageIndex);
        void renderGameObjects(VkCommandBuffer commandBuffer);
        void updateAnimation(float timeSeconds);

        Window                       window{width(), height(), "Engine App"};
        Device                       device{window};
        std::unique_ptr<SwapChain>   swapChain;
        std::unique_ptr<Pipeline>    pipeline;
        VkPipelineLayout             pipelineLayout;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<GameObject>      gameObjects;
        std::vector<AnimationTrack>  animationTracks;
    };

} // namespace engine