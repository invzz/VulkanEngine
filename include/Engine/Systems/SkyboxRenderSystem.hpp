#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Skybox.hpp"

namespace engine {

  struct SkyboxSettings
  {
    bool      useProcedural{false};
    glm::vec4 sunDirection{0.0f, 1.0f, 0.0f, 1.0f}; // w = intensity
    glm::vec4 sunColor{1.0f, 1.0f, 1.0f, 1.0f};
    float     rayleigh{1.0f};
    float     mie{0.02f};
    float     mieEccentricity{0.76f};
  };

  struct FogSettings
  {
    float     density{0.005f};
    float     height{0.0f};
    float     heightDensity{0.1f};
    bool      useSkyColor{true};
    glm::vec3 color{0.5f, 0.6f, 0.7f};

    // God Rays
    bool  enableGodRays{true};
    float godRayDensity{1.0f};
    float godRayWeight{0.01f};
    float godRayDecay{0.97f};
    float godRayExposure{0.5f};
  };

  /**
   * @brief Render system for skybox/environment maps
   *
   * Renders a cubemap skybox as the background of the scene.
   * Should be rendered first (or last with depth write disabled).
   */
  class SkyboxRenderSystem
  {
  public:
    SkyboxRenderSystem(Device& device, VkRenderPass renderPass);
    ~SkyboxRenderSystem();

    // Non-copyable
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
    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    void createProceduralPipeline(VkRenderPass renderPass);

    Device& device_;

    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<Pipeline> proceduralPipeline_;

    VkPipelineLayout      pipelineLayout_           = VK_NULL_HANDLE;
    VkPipelineLayout      proceduralPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_      = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_           = VK_NULL_HANDLE;

    // Pre-allocated descriptor sets per frame
    std::vector<VkDescriptorSet> descriptorSets_;
  };

} // namespace engine
