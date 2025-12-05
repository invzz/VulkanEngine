#pragma once

#include <vulkan/vulkan.h>

#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Skybox.hpp"

namespace engine {

  /**
   * @brief Image-Based Lighting (IBL) System
   *
   * Generates IBL textures from an environment cubemap for PBR ambient lighting:
   * - Irradiance Map: Diffuse ambient lighting (convolved hemisphere)
   * - Prefiltered Environment Map: Specular ambient lighting (mip-mapped by roughness)
   * - BRDF LUT: 2D lookup texture for split-sum approximation
   */
  class IBLSystem
  {
  public:
    struct Settings
    {
      int   irradianceSize        = 64;
      int   prefilterSize         = 512;
      int   prefilterMipLevels    = 9;
      int   brdfLUTSize           = 512;
      int   prefilterSampleCount  = 4096;
      float irradianceSampleDelta = 0.025f;
    };

    IBLSystem(Device& device);
    ~IBLSystem();

    // Non-copyable
    IBLSystem(const IBLSystem&)            = delete;
    IBLSystem& operator=(const IBLSystem&) = delete;

    /**
     * @brief Generate all IBL textures from environment cubemap
     * @param skybox Source environment cubemap
     */
    void generateFromSkybox(Skybox& skybox);

    void requestRegeneration(const Settings& settings, Skybox& skybox);
    void update();

    void            updateSettings(const Settings& settings);
    const Settings& getSettings() const { return settings_; }

    /**
     * @brief Check if IBL textures have been generated
     */
    bool isGenerated() const { return generated_; }

    // Accessors for descriptor binding
    VkDescriptorImageInfo getIrradianceDescriptorInfo() const;
    VkDescriptorImageInfo getPrefilteredDescriptorInfo() const;
    VkDescriptorImageInfo getBRDFLUTDescriptorInfo() const;

  private:
    Settings settings_;
    void     createIrradianceMap();
    void     createPrefilteredEnvMap();
    void     createBRDFLUT();

    void generateIrradianceMap(Skybox& skybox);
    void generatePrefilteredEnvMap(Skybox& skybox);
    void generateBRDFLUT();

    void createIrradianceResources();
    void createPrefilterResources();
    void createBRDFResources();

    void cleanup();

    Device& device_;
    bool    generated_ = false;

    // Irradiance cubemap
    VkImage        irradianceImage_     = VK_NULL_HANDLE;
    VkDeviceMemory irradianceMemory_    = VK_NULL_HANDLE;
    VkImageView    irradianceImageView_ = VK_NULL_HANDLE;
    VkSampler      irradianceSampler_   = VK_NULL_HANDLE;

    // Prefiltered environment cubemap
    VkImage        prefilteredImage_     = VK_NULL_HANDLE;
    VkDeviceMemory prefilteredMemory_    = VK_NULL_HANDLE;
    VkImageView    prefilteredImageView_ = VK_NULL_HANDLE;
    VkSampler      prefilteredSampler_   = VK_NULL_HANDLE;

    // BRDF integration LUT
    VkImage        brdfLUTImage_     = VK_NULL_HANDLE;
    VkDeviceMemory brdfLUTMemory_    = VK_NULL_HANDLE;
    VkImageView    brdfLUTImageView_ = VK_NULL_HANDLE;
    VkSampler      brdfLUTSampler_   = VK_NULL_HANDLE;

    // Pipeline resources for irradiance convolution
    VkRenderPass          irradianceRenderPass_     = VK_NULL_HANDLE;
    VkPipelineLayout      irradiancePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline            irradiancePipeline_       = VK_NULL_HANDLE;
    VkDescriptorSetLayout irradianceDescSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorPool      irradianceDescPool_       = VK_NULL_HANDLE;
    VkDescriptorSet       irradianceDescSet_        = VK_NULL_HANDLE;

    // Pipeline resources for prefilter convolution
    VkRenderPass          prefilterRenderPass_     = VK_NULL_HANDLE;
    VkPipelineLayout      prefilterPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline            prefilterPipeline_       = VK_NULL_HANDLE;
    VkDescriptorSetLayout prefilterDescSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorPool      prefilterDescPool_       = VK_NULL_HANDLE;
    VkDescriptorSet       prefilterDescSet_        = VK_NULL_HANDLE;

    // Pipeline resources for BRDF LUT computation
    VkPipelineLayout      brdfPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline            brdfPipeline_       = VK_NULL_HANDLE;
    VkDescriptorSetLayout brdfDescSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorPool      brdfDescPool_       = VK_NULL_HANDLE;
    VkDescriptorSet       brdfDescSet_        = VK_NULL_HANDLE;

    // Deferred regeneration state
    bool     regenerationRequested_ = false;
    Settings nextSettings_;
    Skybox*  nextSkybox_ = nullptr;
  };

} // namespace engine
