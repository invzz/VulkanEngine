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
        bool proceduralSky{true};
        bool useSkyLUT{true};
        float timeOfDay{12.0f};       // 0-24 hours
        float latitude{0.0f};          // observer latitude in degrees [-90, 90]
        int   dayOfYear{172};          // 1-365, drives seasonal declination
        float skyIntensity{1.0f};
        SkyMode skyMode{SkyMode::None};
        // Atmospheric scattering coefficients (per-channel: R, G, B)
        glm::dvec3 betaRayleigh{5.5e-6, 13.0e-6, 22.4e-6};
        glm::dvec3 betaMie{21.0e-6, 21.0e-6, 21.0e-6};
        float mieG{0.76f};
        // Scale heights (meters)
        double atmosphereRadius{6460e3};
        double rayleighScaleHeight{8000.0};
        double mieScaleHeight{1200.0};
        // Sun intensity
        float sunIntensity{6.0f};
    };
    // Push-constant layout shared by skybox_fullscreen.vert/.frag and the
    // ProceduralSkyCapture cubemap baker. Must match the GLSL `PushConstants`.
    struct SkyboxPushConstants {
        glm::mat4 viewProjection;
        glm::vec4 debugParams;      // x = debugCubemapFaces, y = proceduralSky, z = useSkyLUT, w = captureToCubemap
        glm::vec4 sunDirection;     // xyz = direction to sun, w = unused
        glm::vec4 sunColor;         // rgb = sun color, w = sun angular radius (radians, default 0.015)
        glm::vec4 skyParams;        // x = timeOfDay (0-24), y = intensity, zw = unused
        int       faceIndex{0};      // cube face when debugParams.w > 0.5
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
        void                                    createSkyLUTResources();
        void                                    createSkyLUTComputeResources();
        void                                    updateSkyLUTIfNeeded(const SkyboxSettings& settings, const glm::vec3& sunDirection);
        Device&                                 device_;
        std::unique_ptr<Pipeline>               pipeline_;
        std::unique_ptr<Skybox>                 fallbackSkybox_;
        VkPipelineLayout                        pipelineLayout_      = VK_NULL_HANDLE;
        VkDescriptorSetLayout                   descriptorSetLayout_ = VK_NULL_HANDLE;
        std::unique_ptr<engine::DescriptorPool> descriptorPool_;
        std::vector<VkDescriptorSet>            descriptorSets_;
        VkImage                                 skyLUTImage_              = VK_NULL_HANDLE;
        VkDeviceMemory                          skyLUTMemory_             = VK_NULL_HANDLE;
        VkImageView                             skyLUTImageView_          = VK_NULL_HANDLE;
        VkSampler                               skyLUTSampler_            = VK_NULL_HANDLE;
        VkPipelineLayout                        skyLUTComputeLayout_      = VK_NULL_HANDLE;
        VkPipeline                              skyLUTComputePipeline_    = VK_NULL_HANDLE;
        VkDescriptorSetLayout                   skyLUTComputeSetLayout_   = VK_NULL_HANDLE;
        VkDescriptorPool                        skyLUTComputePool_        = VK_NULL_HANDLE;
        VkDescriptorSet                         skyLUTComputeSet_         = VK_NULL_HANDLE;
        float                                   skyLUTLastTimeOfDay_      = -1000.0f;
        float                                   skyLUTLastLatitude_       = 1e9f;
        int                                     skyLUTLastDayOfYear_      = -1;
        bool                                    skyLUTReady_              = false;
        bool                                    skyLUTInGeneralLayout_    = false;
        static constexpr uint32_t               kSkyLUTWidth              = 256;
        static constexpr uint32_t               kSkyLUTHeight             = 128;
    };
}  // namespace engine
#endif
