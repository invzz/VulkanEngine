#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBLSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBLSYSTEM_HPP

#include <vulkan/vulkan.h>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Skybox.hpp"

namespace engine {

  class Pipeline;

  /**
   * @brief Image-Based Lighting (IBL) System
   *
   * Generates IBL textures from an environment cubemap for PBR ambient lighting:
   * - Irradiance Map: Diffuse ambient lighting (convolved hemisphere)
   * - Prefiltered Environment Map: Specular ambient lighting (mip-mapped by
   * roughness)
   * - BRDF LUT: 2D lookup texture for split-sum approximation
   */
  class IBLSystem
  {
  public:
    struct Settings
    {
      int   irradianceSize        = 64;
      int   prefilterSize         = 256;
      int   prefilterMipLevels    = 8;
      int   brdfLUTSize           = 256;
      int   prefilterSampleCount  = 1024;
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

    // Preferred workflow: load prebaked IBL assets from disk.
    // Directory convention (recommended):
    //   <dir>/irradiance.vtex
    //   <dir>/prefilter.vtex
    //   <dir>/brdf_lut.vtex
    [[nodiscard]] bool loadFromDisk(const std::string& directory);
    [[nodiscard]] bool saveToDisk(const std::string& directory) const;

    void requestRegeneration(const Settings& settings, Skybox& skybox);
    void update();

    void                          updateSettings(const Settings& settings);
    [[nodiscard]] const Settings& getSettings() const { return settings_; }

    /**
     * @brief Check if IBL textures have been generated
     */
    [[nodiscard]] bool isGenerated() const { return generated_; }

    // Incremented whenever the underlying IBL image views/samplers change.
    // Useful for callers to refresh descriptor sets after regeneration.
    [[nodiscard]] uint64_t getGenerationCounter() const { return generationCounter_; }

    // Accessors for descriptor binding
    [[nodiscard]] VkDescriptorImageInfo getIrradianceDescriptorInfo() const;
    [[nodiscard]] VkDescriptorImageInfo getPrefilteredDescriptorInfo() const;
    [[nodiscard]] VkDescriptorImageInfo getBRDFLUTDescriptorInfo() const;

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
    void createFallbackResources();

    // Ensure BRDF LUT exists (it is environment-independent and should be created once).
    void ensureBRDFLUT();

    Device&  device_;
    bool     generated_         = false;
    uint64_t generationCounter_ = 0;

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

    // Tracks the actual allocated size for brdfLUTImage_ (fallback is 1).
    int brdfLUTCurrentSize_ = 0;

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

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBLSYSTEM_HPP
