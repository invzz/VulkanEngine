#include "Engine/Systems/IBLSystem.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Systems/IBL/IBLHelpers.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

namespace engine {

  IBLSystem::IBLSystem(Device& device) : device_{device}
  {
    // Ensure descriptor bindings are always valid even before any environment skybox is loaded.
    // This creates tiny black fallback textures (irradiance/prefilter cubemaps + BRDF LUT).
    createFallbackResources();
  }

  IBLSystem::~IBLSystem()
  {
    cleanup();
  }

  VkDescriptorImageInfo IBLSystem::getIrradianceDescriptorInfo() const
  {
    return VkDescriptorImageInfo{
            .sampler     = irradianceSampler_,
            .imageView   = irradianceImageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo IBLSystem::getPrefilteredDescriptorInfo() const
  {
    return VkDescriptorImageInfo{
            .sampler     = prefilteredSampler_,
            .imageView   = prefilteredImageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo IBLSystem::getBRDFLUTDescriptorInfo() const
  {
    return VkDescriptorImageInfo{
            .sampler     = brdfLUTSampler_,
            .imageView   = brdfLUTImageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  void IBLSystem::updateSettings(const Settings& settings)
  {
    settings_ = settings;
  }

  void IBLSystem::requestRegeneration(const Settings& newSettings, Skybox& skybox)
  {
    nextSettings_          = newSettings;
    nextSkybox_            = &skybox;
    regenerationRequested_ = true;
  }

  void IBLSystem::update()
  {
    if (regenerationRequested_ && (nextSkybox_ != nullptr))
    {
      // Update settings
      settings_ = nextSettings_;

      // Regenerate
      generateFromSkybox(*nextSkybox_);

      // Reset flag
      regenerationRequested_ = false;
      nextSkybox_            = nullptr;
    }
  }

  void IBLSystem::generateFromSkybox(Skybox& skybox)
  {
    // Industry-standard runtime behavior:
    // - BRDF LUT is global/static (generate once per device/settings)
    // - Irradiance/prefilter depend on the environment

    ensureBRDFLUT();

    // Drop only the environment-dependent resources.
    ibl_detail::deferDestroySampler(device_, irradianceSampler_);
    ibl_detail::deferDestroyImageView(device_, irradianceImageView_);
    ibl_detail::deferDestroyImage(device_, irradianceImage_);
    ibl_detail::deferFreeMemory(device_, irradianceMemory_);

    ibl_detail::deferDestroySampler(device_, prefilteredSampler_);
    ibl_detail::deferDestroyImageView(device_, prefilteredImageView_);
    ibl_detail::deferDestroyImage(device_, prefilteredImage_);
    ibl_detail::deferFreeMemory(device_, prefilteredMemory_);

    createIrradianceMap();
    createPrefilteredEnvMap();

    createIrradianceResources();
    generateIrradianceMap(skybox);

    createPrefilterResources();
    generatePrefilteredEnvMap(skybox);

    generated_ = true;

    generationCounter_++;
  }

  bool IBLSystem::saveToDisk(const std::string& directory) const
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(directory), ec);
    if (ec)
    {
      return false;
    }

    // Note: sizes are implicit from settings for generated images.
    // Irradiance/prefilter are cubemaps with 6 layers.
    bool           ok = true;
    fs::path const dirPath(directory);
    ok = ok && ibl_detail::vtex::writeImage(device_,
                                            (dirPath / "irradiance.vtex").generic_string(),
                                            irradianceImage_,
                                            VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(settings_.irradianceSize),
                                            static_cast<uint32_t>(settings_.irradianceSize),
                                            1,
                                            6);
    ok = ok && ibl_detail::vtex::writeImage(device_,
                                            (dirPath / "prefilter.vtex").generic_string(),
                                            prefilteredImage_,
                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                            static_cast<uint32_t>(settings_.prefilterSize),
                                            static_cast<uint32_t>(settings_.prefilterSize),
                                            static_cast<uint32_t>(settings_.prefilterMipLevels),
                                            6);
    ok = ok && ibl_detail::vtex::writeImage(device_,
                                            (dirPath / "brdf_lut.vtex").generic_string(),
                                            brdfLUTImage_,
                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                            static_cast<uint32_t>(settings_.brdfLUTSize),
                                            static_cast<uint32_t>(settings_.brdfLUTSize),
                                            1,
                                            1);
    return ok;
  }

  bool IBLSystem::loadFromDisk(const std::string& directory)
  {
    namespace fs = std::filesystem;
    fs::path const dirPath(directory);

    ibl_detail::vtex::Header irrH{};
    ibl_detail::vtex::Header preH{};
    ibl_detail::vtex::Header brdfH{};

    bool ok = true;
    ok      = ok && ibl_detail::vtex::loadImage(device_,
                                           (dirPath / "irradiance.vtex").generic_string(),
                                           irradianceImage_,
                                           irradianceMemory_,
                                           irradianceImageView_,
                                           irradianceSampler_,
                                           VK_IMAGE_VIEW_TYPE_CUBE,
                                           VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                           &irrH);
    ok      = ok && ibl_detail::vtex::loadImage(device_,
                                           (dirPath / "prefilter.vtex").generic_string(),
                                           prefilteredImage_,
                                           prefilteredMemory_,
                                           prefilteredImageView_,
                                           prefilteredSampler_,
                                           VK_IMAGE_VIEW_TYPE_CUBE,
                                           VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                           &preH);
    ok = ok && ibl_detail::vtex::loadImage(device_, (dirPath / "brdf_lut.vtex").generic_string(), brdfLUTImage_, brdfLUTMemory_, brdfLUTImageView_, brdfLUTSampler_, VK_IMAGE_VIEW_TYPE_2D, 0, &brdfH);

    if (ok)
    {
      // Keep size bookkeeping in sync for ensureBRDFLUT().
      brdfLUTCurrentSize_ = static_cast<int>(brdfH.width);
    }

    if (ok)
    {
      generated_ = true;
      generationCounter_++;
    }
    return ok;
  }

  void IBLSystem::resetToFallback()
  {
    // Destroy current IBL resources (environment + BRDF LUT), then recreate the tiny black fallbacks.
    // We intentionally reset everything so descriptor infos always point at valid views/samplers.
    ibl_detail::deferDestroySampler(device_, irradianceSampler_);
    ibl_detail::deferDestroyImageView(device_, irradianceImageView_);
    ibl_detail::deferDestroyImage(device_, irradianceImage_);
    ibl_detail::deferFreeMemory(device_, irradianceMemory_);

    ibl_detail::deferDestroySampler(device_, prefilteredSampler_);
    ibl_detail::deferDestroyImageView(device_, prefilteredImageView_);
    ibl_detail::deferDestroyImage(device_, prefilteredImage_);
    ibl_detail::deferFreeMemory(device_, prefilteredMemory_);

    ibl_detail::deferDestroySampler(device_, brdfLUTSampler_);
    ibl_detail::deferDestroyImageView(device_, brdfLUTImageView_);
    ibl_detail::deferDestroyImage(device_, brdfLUTImage_);
    ibl_detail::deferFreeMemory(device_, brdfLUTMemory_);

    brdfLUTCurrentSize_ = 0;

    createFallbackResources();
  }

  void IBLSystem::createFallbackResources()
  {
    // Tiny black textures: fast to create and good defaults when no environment is loaded.
    // Note: include TRANSFER_DST so we can clear them.
    ibl_detail::createImage(device_,
                            1,
                            1,
                            1,
                            VK_FORMAT_R32G32B32A32_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            irradianceImage_,
                            irradianceMemory_,
                            6,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    irradianceImageView_ = ibl_detail::createImageView(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    {
      VkSamplerCreateInfo samplerInfo{};
      samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerInfo.magFilter               = VK_FILTER_LINEAR;
      samplerInfo.minFilter               = VK_FILTER_LINEAR;
      samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.anisotropyEnable        = VK_FALSE;
      samplerInfo.maxAnisotropy           = 1.0f;
      samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      samplerInfo.unnormalizedCoordinates = VK_FALSE;
      samplerInfo.compareEnable           = VK_FALSE;
      samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
      samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.mipLodBias              = 0.0f;
      samplerInfo.minLod                  = 0.0f;
      samplerInfo.maxLod                  = 0.0f;

      if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &irradianceSampler_) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create fallback irradiance sampler!");
      }
    }

    ibl_detail::createImage(device_,
                            1,
                            1,
                            1,
                            VK_FORMAT_R16G16B16A16_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            prefilteredImage_,
                            prefilteredMemory_,
                            6,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    prefilteredImageView_ = ibl_detail::createImageView(device_, prefilteredImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    {
      VkSamplerCreateInfo samplerInfo{};
      samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerInfo.magFilter               = VK_FILTER_LINEAR;
      samplerInfo.minFilter               = VK_FILTER_LINEAR;
      samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.anisotropyEnable        = VK_FALSE;
      samplerInfo.maxAnisotropy           = 1.0f;
      samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      samplerInfo.unnormalizedCoordinates = VK_FALSE;
      samplerInfo.compareEnable           = VK_FALSE;
      samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
      samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.mipLodBias              = 0.0f;
      samplerInfo.minLod                  = 0.0f;
      samplerInfo.maxLod                  = 0.0f;

      if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &prefilteredSampler_) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create fallback prefilter sampler!");
      }
    }

    ibl_detail::createImage(device_,
                            1,
                            1,
                            1,
                            VK_FORMAT_R16G16_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            brdfLUTImage_,
                            brdfLUTMemory_);

    brdfLUTCurrentSize_ = 1;

    brdfLUTImageView_ = ibl_detail::createImageView(device_, brdfLUTImage_, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D);

    {
      VkSamplerCreateInfo samplerInfo{};
      samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerInfo.magFilter               = VK_FILTER_LINEAR;
      samplerInfo.minFilter               = VK_FILTER_LINEAR;
      samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.anisotropyEnable        = VK_FALSE;
      samplerInfo.maxAnisotropy           = 1.0f;
      samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      samplerInfo.unnormalizedCoordinates = VK_FALSE;
      samplerInfo.compareEnable           = VK_FALSE;
      samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
      samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.mipLodBias              = 0.0f;
      samplerInfo.minLod                  = 0.0f;
      samplerInfo.maxLod                  = 0.0f;

      if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &brdfLUTSampler_) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create fallback brdf LUT sampler!");
      }
    }

    // Clear all fallback images to black and transition to shader read.
    VkClearColorValue const clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};

    auto clearImage = [&](VkImage image, uint32_t mipLevels, uint32_t layers) {
      ibl_detail::transitionImageLayout(device_, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, layers);

      VkCommandBuffer cmd = device_.getMemory().beginSingleTimeCommands();

      VkImageSubresourceRange range{};
      range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel   = 0;
      range.levelCount     = mipLevels;
      range.baseArrayLayer = 0;
      range.layerCount     = layers;

      vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

      device_.getMemory().endSingleTimeCommands(cmd);

      ibl_detail::transitionImageLayout(device_, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, layers);
    };

    clearImage(irradianceImage_, 1, 6);
    clearImage(prefilteredImage_, 1, 6);
    clearImage(brdfLUTImage_, 1, 1);

    generated_ = false;
    generationCounter_++;
  }

  void IBLSystem::cleanup()
  {
    VkDevice dev = device_.device();

    // Cleanup is a hard tear-down and may run while frames are still queued.
    vkDeviceWaitIdle(dev);

    // Ensure any previously deferred destroys are executed before we start tearing down.
    device_.flushAllDeferred();

    generated_ = false;

    // Irradiance resources
    if (irradianceSampler_ != nullptr)
    {
      vkDestroySampler(dev, irradianceSampler_, nullptr);
      irradianceSampler_ = VK_NULL_HANDLE;
    }
    if (irradianceImageView_ != nullptr)
    {
      vkDestroyImageView(dev, irradianceImageView_, nullptr);
      irradianceImageView_ = VK_NULL_HANDLE;
    }
    if (irradianceImage_ != nullptr)
    {
      vkDestroyImage(dev, irradianceImage_, nullptr);
      irradianceImage_ = VK_NULL_HANDLE;
    }
    if (irradianceMemory_ != nullptr)
    {
      vkFreeMemory(dev, irradianceMemory_, nullptr);
      irradianceMemory_ = VK_NULL_HANDLE;
    }
    if (irradiancePipeline_ != nullptr)
    {
      vkDestroyPipeline(dev, irradiancePipeline_, nullptr);
      irradiancePipeline_ = VK_NULL_HANDLE;
    }
    if (irradiancePipelineLayout_ != nullptr)
    {
      vkDestroyPipelineLayout(dev, irradiancePipelineLayout_, nullptr);
      irradiancePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (irradianceRenderPass_ != nullptr)
    {
      vkDestroyRenderPass(dev, irradianceRenderPass_, nullptr);
      irradianceRenderPass_ = VK_NULL_HANDLE;
    }
    if (irradianceDescPool_ != nullptr)
    {
      vkDestroyDescriptorPool(dev, irradianceDescPool_, nullptr);
      irradianceDescPool_ = VK_NULL_HANDLE;
    }
    if (irradianceDescSetLayout_ != nullptr)
    {
      vkDestroyDescriptorSetLayout(dev, irradianceDescSetLayout_, nullptr);
      irradianceDescSetLayout_ = VK_NULL_HANDLE;
    }
    irradianceDescSet_ = VK_NULL_HANDLE;

    // Prefilter resources
    if (prefilteredSampler_ != nullptr)
    {
      vkDestroySampler(dev, prefilteredSampler_, nullptr);
      prefilteredSampler_ = VK_NULL_HANDLE;
    }
    if (prefilteredImageView_ != nullptr)
    {
      vkDestroyImageView(dev, prefilteredImageView_, nullptr);
      prefilteredImageView_ = VK_NULL_HANDLE;
    }
    if (prefilteredImage_ != nullptr)
    {
      vkDestroyImage(dev, prefilteredImage_, nullptr);
      prefilteredImage_ = VK_NULL_HANDLE;
    }
    if (prefilteredMemory_ != nullptr)
    {
      vkFreeMemory(dev, prefilteredMemory_, nullptr);
      prefilteredMemory_ = VK_NULL_HANDLE;
    }
    if (prefilterPipeline_ != nullptr)
    {
      vkDestroyPipeline(dev, prefilterPipeline_, nullptr);
      prefilterPipeline_ = VK_NULL_HANDLE;
    }
    if (prefilterPipelineLayout_ != nullptr)
    {
      vkDestroyPipelineLayout(dev, prefilterPipelineLayout_, nullptr);
      prefilterPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (prefilterRenderPass_ != nullptr)
    {
      vkDestroyRenderPass(dev, prefilterRenderPass_, nullptr);
      prefilterRenderPass_ = VK_NULL_HANDLE;
    }
    if (prefilterDescPool_ != nullptr)
    {
      vkDestroyDescriptorPool(dev, prefilterDescPool_, nullptr);
      prefilterDescPool_ = VK_NULL_HANDLE;
    }
    if (prefilterDescSetLayout_ != nullptr)
    {
      vkDestroyDescriptorSetLayout(dev, prefilterDescSetLayout_, nullptr);
      prefilterDescSetLayout_ = VK_NULL_HANDLE;
    }
    prefilterDescSet_ = VK_NULL_HANDLE;

    // BRDF LUT resources
    if (brdfLUTSampler_ != nullptr)
    {
      vkDestroySampler(dev, brdfLUTSampler_, nullptr);
      brdfLUTSampler_ = VK_NULL_HANDLE;
    }
    if (brdfLUTImageView_ != nullptr)
    {
      vkDestroyImageView(dev, brdfLUTImageView_, nullptr);
      brdfLUTImageView_ = VK_NULL_HANDLE;
    }
    if (brdfLUTImage_ != nullptr)
    {
      vkDestroyImage(dev, brdfLUTImage_, nullptr);
      brdfLUTImage_ = VK_NULL_HANDLE;
    }
    if (brdfLUTMemory_ != nullptr)
    {
      vkFreeMemory(dev, brdfLUTMemory_, nullptr);
      brdfLUTMemory_ = VK_NULL_HANDLE;
    }
    if (brdfPipeline_ != nullptr)
    {
      vkDestroyPipeline(dev, brdfPipeline_, nullptr);
      brdfPipeline_ = VK_NULL_HANDLE;
    }
    if (brdfPipelineLayout_ != nullptr)
    {
      vkDestroyPipelineLayout(dev, brdfPipelineLayout_, nullptr);
      brdfPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (brdfDescPool_ != nullptr)
    {
      vkDestroyDescriptorPool(dev, brdfDescPool_, nullptr);
      brdfDescPool_ = VK_NULL_HANDLE;
    }
    if (brdfDescSetLayout_ != nullptr)
    {
      vkDestroyDescriptorSetLayout(dev, brdfDescSetLayout_, nullptr);
      brdfDescSetLayout_ = VK_NULL_HANDLE;
    }
    brdfDescSet_ = VK_NULL_HANDLE;

    generated_ = false;
  }
} // namespace engine
