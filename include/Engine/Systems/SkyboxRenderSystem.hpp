#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SKYBOXRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SKYBOXRENDERSYSTEM_HPP
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
namespace engine {
    class Skybox;
    enum class SkyMode : uint8_t {
        None = 0,
        Procedural = 1,
        Cubemap = 2,
    };
    struct SkyboxSettings {
        bool debugCubemapFaces{false};
        bool proceduralSky{false};
        float timeOfDay{12.0f};       // 0-24 hours
        float skyIntensity{1.0f};
        SkyMode skyMode{SkyMode::None};
    };
    /**
 * @brief Render system for skybox/environment maps
 *
 * Renders a cubemap skybox as the background of the scene.
 * Should be rendered first (or last with depth write disabled).
 */
    class SkyboxRenderSystem {
       public:
        SkyboxRenderSystem(Device& device, VkRenderPass renderPass);
        ~SkyboxRenderSystem();
        SkyboxRenderSystem(const SkyboxRenderSystem&)            = delete;
        SkyboxRenderSystem& operator=(const SkyboxRenderSystem&) = delete;
        /**
   * @brief Render the skybox
   * @param frameInfo Current frame information (camera, etc.)
   * @param skybox The skybox cubemap to render (can be null if using procedural)
   * @param settings Skybox configuration
   */
        void render(FrameInfo& frameInfo, Skybox* skybox, const SkyboxSettings& settings);

       private:
        void                                    createDescriptorSetLayout();
        void                                    createPipelineLayout();
        void                                    createPipeline(VkRenderPass renderPass);
        Device&                                 device_;
        std::unique_ptr<Pipeline>               pipeline_;
        VkPipelineLayout                        pipelineLayout_      = VK_NULL_HANDLE;
        VkDescriptorSetLayout                   descriptorSetLayout_ = VK_NULL_HANDLE;
        std::unique_ptr<engine::DescriptorPool> descriptorPool_;
        std::vector<VkDescriptorSet>            descriptorSets_;
    };
}  // namespace engine
#endif
