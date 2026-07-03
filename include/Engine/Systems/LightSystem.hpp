#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTSYSTEM_HPP

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

    class Scene;

    class LightSystem {
       public:
        LightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~LightSystem();

        LightSystem(const LightSystem&)            = delete;
        LightSystem& operator=(const LightSystem&) = delete;

        void render(FrameInfo& frameInfo);
        void update(FrameInfo& frameInfo, GlobalUbo& ubo) const;

        static void updateTargetLockedLight(entt::entity entity, Scene* scene);

        static void updateAllTargetLockedLights(Scene& scene);

       private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);
        void createDirectionalLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createDirectionalLightPipeline(VkRenderPass renderPass);
        void createSpotLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createSpotLightPipeline(VkRenderPass renderPass);

        Device& device;

        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout          pipelineLayout;

        std::unique_ptr<Pipeline> directionalPipeline;
        VkPipelineLayout          directionalPipelineLayout;

        std::unique_ptr<Pipeline> spotPipeline;
        VkPipelineLayout          spotPipelineLayout;
    };
}  // namespace engine

#endif
