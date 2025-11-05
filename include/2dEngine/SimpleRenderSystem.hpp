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
#include "SwapChain.hpp"

namespace engine {

    class SimpleRenderSystem
    {
      public:
        SimpleRenderSystem(Device& device, VkRenderPass renderPass);
        ~SimpleRenderSystem();

        // delete copy operations
        SimpleRenderSystem(const SimpleRenderSystem&)            = delete;
        SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

        void renderGameObjects(VkCommandBuffer commandBuffer, const std::vector<GameObject>& gameObjects);

      private:
        void createPipelineLayout();
        void createPipeline(VkRenderPass renderPass);

        Device& device;

        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout          pipelineLayout;
    };
} // namespace engine
