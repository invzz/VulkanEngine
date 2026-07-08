#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_PROCEDURALESKYCAPTURE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_PROCEDURALESKYCAPTURE_HPP
#include <memory>
#include <vulkan/vulkan.h>

#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/SunLight.hpp"

namespace engine {
    // Renders the procedural sky (skybox_fullscreen shader, capture branch) into
    // a cubemap so it can be fed to the existing IBLSystem::generateFromSkybox.
    // The dome is authored Y-up; the capture branch samples each cube face
    // direction directly, so the resulting cubemap matches the world axes used by
    // the surface shaders' IBL sampling (North = -Z, East = +X, Up = +Y).
    class ProceduralSkyCapture {
       public:
        explicit ProceduralSkyCapture(class Device& device);
        ~ProceduralSkyCapture();

        ProceduralSkyCapture(const ProceduralSkyCapture&)            = delete;
        ProceduralSkyCapture& operator=(const ProceduralSkyCapture&) = delete;

        // Bake the procedural sky for the given settings into a fresh cubemap.
        // Re-using the same Skybox allocation across calls keeps GPU churn low;
        // the returned pointer is owned by this capture object.
        Skybox* capture(const struct SkyboxSettings& settings, uint32_t faceSize = 256);

       private:
        void ensurePipeline(uint32_t faceSize);
        void ensureRenderTargets(uint32_t faceSize);

        class Device&       device_;
        std::unique_ptr<class Pipeline> pipeline_;
        VkPipelineLayout    layout_     = VK_NULL_HANDLE;
        VkRenderPass       renderPass_ = VK_NULL_HANDLE;
        VkDescriptorPool   pool_       = VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet    descSet_    = VK_NULL_HANDLE;
        VkSampler          sampler_    = VK_NULL_HANDLE;

        std::unique_ptr<Skybox> target_;
        uint32_t                 targetSize_ = 0;
        bool                     targetTransitioned_ = false;
        std::vector<VkFramebuffer> framebuffers_;
        std::vector<VkImageView>  faceViews_;
    };
}  // namespace engine
#endif
