#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_COLLIDERDEBUGRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_COLLIDERDEBUGRENDERSYSTEM_HPP

#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

    class ColliderDebugRenderSystem {
       public:
        ColliderDebugRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~ColliderDebugRenderSystem();

        ColliderDebugRenderSystem(const ColliderDebugRenderSystem&)            = delete;
        ColliderDebugRenderSystem& operator=(const ColliderDebugRenderSystem&) = delete;

        void render(FrameInfo& frameInfo) const;

       private:
        enum class ShapeType : int {
            Box     = 0,
            Sphere  = 1,
            Capsule = 2,
        };

        struct PushConstantData {
            glm::mat4 modelMatrix{1.0f};
            glm::vec4 color{0.0f, 1.0f, 1.0f, 1.0f};
            glm::vec4 shapeParams{0.5f, 0.5f, 0.5f, 0.0f};
            int       shapeType{0};
        };

        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        static glm::mat4 makeNoScaleModelMatrix(const TransformComponent& transform);

        Device&                   device_;
        std::unique_ptr<Pipeline> pipeline_;
        VkPipelineLayout          pipelineLayout_ = VK_NULL_HANDLE;
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_COLLIDERDEBUGRENDERSYSTEM_HPP
