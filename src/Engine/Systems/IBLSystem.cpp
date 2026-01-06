#include "Engine/Systems/IBLSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  namespace {
    void createImageHelper(Device&               device,
                           uint32_t              width,
                           uint32_t              height,
                           uint32_t              mipLevels,
                           VkFormat              format,
                           VkImageTiling         tiling,
                           VkImageUsageFlags     usage,
                           VkMemoryPropertyFlags properties,
                           VkImage&              image,
                           VkDeviceMemory&       imageMemory,
                           uint32_t              arrayLayers = 1,
                           VkImageCreateFlags    flags       = 0);

    VkImageView createImageViewHelper(Device&            device,
                                      VkImage            image,
                                      VkFormat           format,
                                      VkImageAspectFlags aspectFlags,
                                      uint32_t           mipLevels,
                                      VkImageViewType    viewType       = VK_IMAGE_VIEW_TYPE_2D,
                                      uint32_t           baseMipLevel   = 0,
                                      uint32_t           layerCount     = 1,
                                      uint32_t           baseArrayLayer = 0);

    void transitionImageLayoutHelper(Device& device, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount = 1);
  } // namespace

  IBLSystem::IBLSystem(Device& device) : device_{device}
  {
    // Ensure descriptor bindings are always valid even before any environment skybox is loaded.
    // This creates tiny black fallback textures (irradiance/prefilter cubemaps + BRDF LUT).
    createFallbackResources();

    // BRDF LUT is environment-independent; keep it available even when we haven't loaded an environment yet.
    // (Fallback creates a tiny placeholder; this upgrades it to the configured size lazily when needed.)
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
    if (irradianceSampler_ != VK_NULL_HANDLE)
    {
      vkDestroySampler(device_.device(), irradianceSampler_, nullptr);
      irradianceSampler_ = VK_NULL_HANDLE;
    }
    if (irradianceImageView_ != VK_NULL_HANDLE)
    {
      vkDestroyImageView(device_.device(), irradianceImageView_, nullptr);
      irradianceImageView_ = VK_NULL_HANDLE;
    }
    if (irradianceImage_ != VK_NULL_HANDLE)
    {
      vkDestroyImage(device_.device(), irradianceImage_, nullptr);
      irradianceImage_ = VK_NULL_HANDLE;
    }
    if (irradianceMemory_ != VK_NULL_HANDLE)
    {
      vkFreeMemory(device_.device(), irradianceMemory_, nullptr);
      irradianceMemory_ = VK_NULL_HANDLE;
    }

    if (prefilteredSampler_ != VK_NULL_HANDLE)
    {
      vkDestroySampler(device_.device(), prefilteredSampler_, nullptr);
      prefilteredSampler_ = VK_NULL_HANDLE;
    }
    if (prefilteredImageView_ != VK_NULL_HANDLE)
    {
      vkDestroyImageView(device_.device(), prefilteredImageView_, nullptr);
      prefilteredImageView_ = VK_NULL_HANDLE;
    }
    if (prefilteredImage_ != VK_NULL_HANDLE)
    {
      vkDestroyImage(device_.device(), prefilteredImage_, nullptr);
      prefilteredImage_ = VK_NULL_HANDLE;
    }
    if (prefilteredMemory_ != VK_NULL_HANDLE)
    {
      vkFreeMemory(device_.device(), prefilteredMemory_, nullptr);
      prefilteredMemory_ = VK_NULL_HANDLE;
    }

    createIrradianceMap();
    createPrefilteredEnvMap();

    createIrradianceResources();
    generateIrradianceMap(skybox);

    createPrefilterResources();
    generatePrefilteredEnvMap(skybox);

    generated_ = true;

    generationCounter_++;
  }

  namespace {
    struct VTexHeader
    {
      uint32_t magic      = 0x58455456; // 'VTEX'
      uint32_t version    = 1;
      uint32_t vkFormat   = 0;
      uint32_t width      = 0;
      uint32_t height     = 0;
      uint32_t mipLevels  = 0;
      uint32_t layers     = 0;
      uint32_t bytesPerPx = 0;
    };

    uint32_t bytesPerPixelFor(VkFormat format)
    {
      switch (format)
      {
      case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
      case VK_FORMAT_R16G16_SFLOAT:
        return 4;
      case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
      default:
        break;
      }
      throw std::runtime_error("Unsupported VTEX format for IBL assets");
    }

    std::string joinPath(const std::string& a, const std::string& b)
    {
      std::filesystem::path p = std::filesystem::path(a) / b;
      return p.generic_string();
    }
  } // namespace

  void IBLSystem::ensureBRDFLUT()
  {
    // If we already have a non-fallback LUT at the requested size, keep it.
    // We infer this by checking the current image handle and (cheaply) trusting settings.
    if (brdfLUTImage_ != VK_NULL_HANDLE && brdfLUTSampler_ != VK_NULL_HANDLE && brdfLUTImageView_ != VK_NULL_HANDLE && brdfLUTCurrentSize_ == settings_.brdfLUTSize && settings_.brdfLUTSize > 1)
    {
      return;
    }

    // If fallback exists, release it and create a proper LUT.
    if (brdfLUTSampler_ != VK_NULL_HANDLE)
    {
      vkDestroySampler(device_.device(), brdfLUTSampler_, nullptr);
      brdfLUTSampler_ = VK_NULL_HANDLE;
    }
    if (brdfLUTImageView_ != VK_NULL_HANDLE)
    {
      vkDestroyImageView(device_.device(), brdfLUTImageView_, nullptr);
      brdfLUTImageView_ = VK_NULL_HANDLE;
    }
    if (brdfLUTImage_ != VK_NULL_HANDLE)
    {
      vkDestroyImage(device_.device(), brdfLUTImage_, nullptr);
      brdfLUTImage_ = VK_NULL_HANDLE;
    }
    if (brdfLUTMemory_ != VK_NULL_HANDLE)
    {
      vkFreeMemory(device_.device(), brdfLUTMemory_, nullptr);
      brdfLUTMemory_ = VK_NULL_HANDLE;
    }
    brdfLUTCurrentSize_ = 0;

    createBRDFLUT();
    createBRDFResources();
    generateBRDFLUT();
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

    auto writeImage = [&](const std::string& filename, VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers) -> bool {
      if (image == VK_NULL_HANDLE) return false;

      uint32_t const bpp = bytesPerPixelFor(format);

      std::vector<VkBufferImageCopy> regions;
      regions.reserve(static_cast<size_t>(mipLevels) * static_cast<size_t>(layers));

      VkDeviceSize totalBytes = 0;
      for (uint32_t mip = 0; mip < mipLevels; ++mip)
      {
        uint32_t const     w        = (std::max)(1u, width >> mip);
        uint32_t const     h        = (std::max)(1u, height >> mip);
        VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * bpp;
        totalBytes += mipBytes * layers;
      }

      Buffer staging{device_, 1, static_cast<uint32_t>(totalBytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      VkDeviceSize offset = 0;
      for (uint32_t mip = 0; mip < mipLevels; ++mip)
      {
        uint32_t const     w        = (std::max)(1u, width >> mip);
        uint32_t const     h        = (std::max)(1u, height >> mip);
        VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * bpp;
        for (uint32_t layer = 0; layer < layers; ++layer)
        {
          VkBufferImageCopy region{};
          region.bufferOffset                    = offset;
          region.bufferRowLength                 = 0;
          region.bufferImageHeight               = 0;
          region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
          region.imageSubresource.mipLevel       = mip;
          region.imageSubresource.baseArrayLayer = layer;
          region.imageSubresource.layerCount     = 1;
          region.imageOffset                     = {0, 0, 0};
          region.imageExtent                     = {w, h, 1};
          regions.push_back(region);
          offset += mipBytes;
        }
      }

      // Transition, copy, transition back.
      transitionImageLayoutHelper(device_, image, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipLevels, layers);
      device_.memory().copyImageToBuffer(image, staging.getBuffer(), regions);
      transitionImageLayoutHelper(device_, image, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, layers);

      staging.map();
      void* data = staging.getMappedMemory();

      VTexHeader header;
      header.vkFormat   = static_cast<uint32_t>(format);
      header.width      = width;
      header.height     = height;
      header.mipLevels  = mipLevels;
      header.layers     = layers;
      header.bytesPerPx = bpp;

      std::ofstream out(joinPath(directory, filename), std::ios::binary);
      if (!out) return false;
      out.write(reinterpret_cast<const char*>(&header), sizeof(header));
      out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(totalBytes));
      out.close();

      staging.unmap();
      return static_cast<bool>(out);
    };

    // Note: sizes are implicit from settings for generated images.
    // Irradiance/prefilter are cubemaps with 6 layers.
    bool ok = true;
    ok = ok && writeImage("irradiance.vtex", irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(settings_.irradianceSize), static_cast<uint32_t>(settings_.irradianceSize), 1, 6);
    ok = ok && writeImage("prefilter.vtex",
                          prefilteredImage_,
                          VK_FORMAT_R16G16B16A16_SFLOAT,
                          static_cast<uint32_t>(settings_.prefilterSize),
                          static_cast<uint32_t>(settings_.prefilterSize),
                          static_cast<uint32_t>(settings_.prefilterMipLevels),
                          6);
    ok = ok && writeImage("brdf_lut.vtex", brdfLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<uint32_t>(settings_.brdfLUTSize), static_cast<uint32_t>(settings_.brdfLUTSize), 1, 1);
    return ok;
  }

  bool IBLSystem::loadFromDisk(const std::string& directory)
  {
    auto readFile = [&](const std::string& filename, VTexHeader& outHeader, std::vector<std::byte>& outData) -> bool {
      std::ifstream in(joinPath(directory, filename), std::ios::binary);
      if (!in) return false;
      in.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
      if (!in) return false;
      if (outHeader.magic != 0x58455456 || outHeader.version != 1) return false;

      std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      outData.resize(bytes.size());
      std::memcpy(outData.data(), bytes.data(), bytes.size());
      return true;
    };

    VTexHeader             irrH{};
    VTexHeader             preH{};
    VTexHeader             brdfH{};
    std::vector<std::byte> irrData;
    std::vector<std::byte> preData;
    std::vector<std::byte> brdfData;
    if (!readFile("irradiance.vtex", irrH, irrData)) return false;
    if (!readFile("prefilter.vtex", preH, preData)) return false;
    if (!readFile("brdf_lut.vtex", brdfH, brdfData)) return false;

    auto upload = [&](VkImage&                      image,
                      VkDeviceMemory&               mem,
                      VkImageView&                  view,
                      VkSampler&                    sampler,
                      const VTexHeader&             h,
                      const std::vector<std::byte>& data,
                      VkImageViewType               viewType,
                      VkImageCreateFlags            flags,
                      bool /*cube*/) -> bool {
      VkFormat const format = static_cast<VkFormat>(h.vkFormat);
      if (h.bytesPerPx != bytesPerPixelFor(format)) return false;

      // Destroy previous resources
      if (sampler != VK_NULL_HANDLE)
      {
        vkDestroySampler(device_.device(), sampler, nullptr);
        sampler = VK_NULL_HANDLE;
      }
      if (view != VK_NULL_HANDLE)
      {
        vkDestroyImageView(device_.device(), view, nullptr);
        view = VK_NULL_HANDLE;
      }
      if (image != VK_NULL_HANDLE)
      {
        vkDestroyImage(device_.device(), image, nullptr);
        image = VK_NULL_HANDLE;
      }
      if (mem != VK_NULL_HANDLE)
      {
        vkFreeMemory(device_.device(), mem, nullptr);
        mem = VK_NULL_HANDLE;
      }

      createImageHelper(device_,
                        h.width,
                        h.height,
                        h.mipLevels,
                        format,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        image,
                        mem,
                        h.layers,
                        flags);

      VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      view                      = createImageViewHelper(device_, image, format, aspect, h.mipLevels, viewType, 0, h.layers, 0);

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
      samplerInfo.maxLod                  = static_cast<float>(h.mipLevels);
      if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) return false;

      Buffer staging{device_, 1, static_cast<uint32_t>(data.size()), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      staging.map();
      staging.writeToBuffer((void*)data.data(), data.size());
      staging.unmap();

      std::vector<VkBufferImageCopy> regions;
      regions.reserve(static_cast<size_t>(h.mipLevels) * static_cast<size_t>(h.layers));

      VkDeviceSize offset = 0;
      for (uint32_t mip = 0; mip < h.mipLevels; ++mip)
      {
        uint32_t const     w        = (std::max)(1u, h.width >> mip);
        uint32_t const     ht       = (std::max)(1u, h.height >> mip);
        VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(ht) * h.bytesPerPx;
        for (uint32_t layer = 0; layer < h.layers; ++layer)
        {
          VkBufferImageCopy region{};
          region.bufferOffset                    = offset;
          region.bufferRowLength                 = 0;
          region.bufferImageHeight               = 0;
          region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
          region.imageSubresource.mipLevel       = mip;
          region.imageSubresource.baseArrayLayer = layer;
          region.imageSubresource.layerCount     = 1;
          region.imageOffset                     = {0, 0, 0};
          region.imageExtent                     = {w, ht, 1};
          regions.push_back(region);
          offset += mipBytes;
        }
      }

      transitionImageLayoutHelper(device_, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, h.mipLevels, h.layers);
      device_.memory().copyBufferToImage(staging.getBuffer(), image, regions);
      transitionImageLayoutHelper(device_, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, h.mipLevels, h.layers);
      return true;
    };

    bool ok = true;
    ok      = ok && upload(irradianceImage_, irradianceMemory_, irradianceImageView_, irradianceSampler_, irrH, irrData, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, true);
    ok      = ok && upload(prefilteredImage_, prefilteredMemory_, prefilteredImageView_, prefilteredSampler_, preH, preData, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, true);
    ok      = ok && upload(brdfLUTImage_, brdfLUTMemory_, brdfLUTImageView_, brdfLUTSampler_, brdfH, brdfData, VK_IMAGE_VIEW_TYPE_2D, 0, false);

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

  void IBLSystem::createFallbackResources()
  {
    // Tiny black textures: fast to create and good defaults when no environment is loaded.
    // Note: include TRANSFER_DST so we can clear them.
    createImageHelper(device_,
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

    irradianceImageView_ = createImageViewHelper(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

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

    createImageHelper(device_,
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

    prefilteredImageView_ = createImageViewHelper(device_, prefilteredImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

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

    createImageHelper(device_,
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

    brdfLUTImageView_ = createImageViewHelper(device_, brdfLUTImage_, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D);

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
      transitionImageLayoutHelper(device_, image, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, layers);

      VkCommandBuffer cmd = device_.getMemory().beginSingleTimeCommands();

      VkImageSubresourceRange range{};
      range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel   = 0;
      range.levelCount     = mipLevels;
      range.baseArrayLayer = 0;
      range.layerCount     = layers;

      vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

      device_.getMemory().endSingleTimeCommands(cmd);

      transitionImageLayoutHelper(device_, image, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, layers);
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

  namespace {
    // Helper to create image
    void createImageHelper(Device&               device,
                           uint32_t              width,
                           uint32_t              height,
                           uint32_t              mipLevels,
                           VkFormat              format,
                           VkImageTiling         tiling,
                           VkImageUsageFlags     usage,
                           VkMemoryPropertyFlags properties,
                           VkImage&              image,
                           VkDeviceMemory&       imageMemory,
                           uint32_t              arrayLayers,
                           VkImageCreateFlags    flags)
    {
      VkImageCreateInfo imageInfo{};
      imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType     = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width  = width;
      imageInfo.extent.height = height;
      imageInfo.extent.depth  = 1;
      imageInfo.mipLevels     = mipLevels;
      imageInfo.arrayLayers   = arrayLayers;
      imageInfo.format        = format;
      imageInfo.tiling        = tiling;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage         = usage;
      imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.flags         = flags;

      device.getMemory().createImageWithInfo(imageInfo, properties, image, imageMemory);
    }

    // Helper to create image view
    VkImageView createImageViewHelper(Device&            device,
                                      VkImage            image,
                                      VkFormat           format,
                                      VkImageAspectFlags aspectFlags,
                                      uint32_t           mipLevels,
                                      VkImageViewType    viewType,
                                      uint32_t           baseMipLevel,
                                      uint32_t           layerCount,
                                      uint32_t           baseArrayLayer)
    {
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = image;
      viewInfo.viewType                        = viewType;
      viewInfo.format                          = format;
      viewInfo.subresourceRange.aspectMask     = aspectFlags;
      viewInfo.subresourceRange.baseMipLevel   = baseMipLevel;
      viewInfo.subresourceRange.levelCount     = mipLevels;
      viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
      viewInfo.subresourceRange.layerCount     = layerCount;

      VkImageView imageView;
      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create texture image view!");
      }
      return imageView;
    }

    // Helper to transition image layout
    void transitionImageLayoutHelper(Device& device, VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount)
    {
      VkCommandBuffer commandBuffer = device.getMemory().beginSingleTimeCommands();

      VkImageMemoryBarrier barrier{};
      barrier.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = oldLayout;

      barrier.newLayout                       = newLayout;
      barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                           = image;
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel   = 0;
      barrier.subresourceRange.levelCount     = mipLevels;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = layerCount;

      VkPipelineStageFlags sourceStage;
      VkPipelineStageFlags destinationStage;

      if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
      {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
      {
        // Readback path for VTEX export:
        // Although the image is currently in SHADER_READ_ONLY_OPTIMAL, its most recent writer may have been
        // a compute shader (BRDF LUT) or a color attachment pass (irradiance/prefilter) and there may not
        // have been an actual shader-read between generation and readback.
        // Include common write access masks/stages so transfer reads observe the generated contents.
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
      {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      }
      else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
      else
      {
        throw std::invalid_argument("unsupported layout transition!");
      }

      vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      device.getMemory().endSingleTimeCommands(commandBuffer);
    }

  } // namespace

  void IBLSystem::createIrradianceMap()
  {
    createImageHelper(device_,
                      settings_.irradianceSize,
                      settings_.irradianceSize,
                      1,
                      VK_FORMAT_R32G32B32A32_SFLOAT,
                      VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      irradianceImage_,
                      irradianceMemory_,
                      6,
                      VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    irradianceImageView_ = createImageViewHelper(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device_.getProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 1.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &irradianceSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance sampler!");
    }

    // Transition to color attachment optimal
    transitionImageLayoutHelper(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 6);
  }

  void IBLSystem::createPrefilteredEnvMap()
  {
    createImageHelper(device_,
                      settings_.prefilterSize,
                      settings_.prefilterSize,
                      settings_.prefilterMipLevels,
                      VK_FORMAT_R16G16B16A16_SFLOAT,
                      VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      prefilteredImage_,
                      prefilteredMemory_,
                      6,
                      VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    prefilteredImageView_ = createImageViewHelper(device_, prefilteredImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, settings_.prefilterMipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device_.getProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(settings_.prefilterMipLevels);

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &prefilteredSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter sampler!");
    }

    // Transition to color attachment optimal
    transitionImageLayoutHelper(device_, prefilteredImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, settings_.prefilterMipLevels, 6);
  }

  void IBLSystem::createBRDFLUT()
  {
    createImageHelper(device_,
                      settings_.brdfLUTSize,
                      settings_.brdfLUTSize,
                      1,
                      VK_FORMAT_R16G16B16A16_SFLOAT,
                      VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      brdfLUTImage_,
                      brdfLUTMemory_);

    brdfLUTImageView_ = createImageViewHelper(device_, brdfLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

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
    samplerInfo.maxLod                  = 1.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &brdfLUTSampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF LUT sampler!");
    }

    brdfLUTCurrentSize_ = settings_.brdfLUTSize;

    // Transition to general layout for compute shader storage
    transitionImageLayoutHelper(device_, brdfLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
  }

  void IBLSystem::createIrradianceResources()
  {
    // Render Pass
    VkAttachmentDescription attachment{};
    attachment.format         = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &attachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &irradianceRenderPass_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance render pass!");
    }

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &irradianceDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance descriptor set layout!");
    }

    // Pipeline Layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4) + sizeof(int) + sizeof(float); // ViewProj + FaceIndex + SampleDelta

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &irradianceDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &irradiancePipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance pipeline layout!");
    }

    // Pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass                        = irradianceRenderPass_;
    pipelineConfig.pipelineLayout                    = irradiancePipelineLayout_;
    pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE; // No culling for full screen quad/cube
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    // No vertex input needed (generated in shader)
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    auto vertCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.vert.spv)");
    auto fragCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(irradiance_convolution.frag.spv)");

    VkShaderModule vertModule;
    VkShaderModule fragModule;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &vertModule);

    createInfo.codeSize = fragCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &fragModule);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module              = vertModule;
    shaderStages[0].pName               = "main";
    shaderStages[0].flags               = 0;
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[0].pNext               = nullptr;

    shaderStages[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module              = fragModule;
    shaderStages[1].pName               = "main";
    shaderStages[1].flags               = 0;
    shaderStages[1].pSpecializationInfo = nullptr;
    shaderStages[1].pNext               = nullptr;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
    pipelineInfo.pViewportState      = &pipelineConfig.viewportInfo;
    pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
    pipelineInfo.pMultisampleState   = &pipelineConfig.multisampleInfo;
    pipelineInfo.pColorBlendState    = &pipelineConfig.colorBlendInfo;
    pipelineInfo.pDepthStencilState  = &pipelineConfig.depthStencilInfo;
    pipelineInfo.pDynamicState       = &pipelineConfig.dynamicStateInfo;
    pipelineInfo.layout              = irradiancePipelineLayout_;
    pipelineInfo.renderPass          = irradianceRenderPass_;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &irradiancePipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance pipeline!");
    }

    vkDestroyShaderModule(device_.device(), vertModule, nullptr);
    vkDestroyShaderModule(device_.device(), fragModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &irradianceDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create irradiance descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = irradianceDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &irradianceDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &irradianceDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate irradiance descriptor set!");
    }
  }

  void IBLSystem::generateIrradianceMap(Skybox& skybox)
  {
    // Update descriptor set
    VkDescriptorImageInfo const imageInfo = skybox.getDescriptorInfo();
    VkWriteDescriptorSet        descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = irradianceDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    glm::mat4 const captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 const captureViews[]    = {glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),   // Top
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)), // Bottom
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkImageView>   imageViews;

    for (int i = 0; i < 6; ++i)
    {
      // Create view for this face
      VkImageView           faceView;
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = irradianceImage_;
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = 1;
      viewInfo.subresourceRange.baseArrayLayer = i;
      viewInfo.subresourceRange.layerCount     = 1;

      vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView);
      imageViews.push_back(faceView);

      // Create framebuffer
      VkFramebuffer           framebuffer;
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = irradianceRenderPass_;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments    = &faceView;
      framebufferInfo.width           = settings_.irradianceSize;
      framebufferInfo.height          = settings_.irradianceSize;
      framebufferInfo.layers          = 1;

      vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer);
      framebuffers.push_back(framebuffer);

      // Render
      VkRenderPassBeginInfo renderPassInfo{};
      renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass        = irradianceRenderPass_;
      renderPassInfo.framebuffer       = framebuffer;
      renderPassInfo.renderArea.offset = {0, 0};
      renderPassInfo.renderArea.extent = {static_cast<uint32_t>(settings_.irradianceSize), static_cast<uint32_t>(settings_.irradianceSize)};

      VkClearValue const clearValue  = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues    = &clearValue;

      vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport{};
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = static_cast<float>(settings_.irradianceSize);
      viewport.height   = static_cast<float>(settings_.irradianceSize);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.offset = {0, 0};
      scissor.extent = {static_cast<uint32_t>(settings_.irradianceSize), static_cast<uint32_t>(settings_.irradianceSize)};
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline_);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipelineLayout_, 0, 1, &irradianceDescSet_, 0, nullptr);

      struct PushBlock
      {
        glm::mat4 mvp;
        int       faceIndex;
        float     sampleDelta;
      } pushBlock;

      pushBlock.mvp         = captureProjection * captureViews[i];
      pushBlock.faceIndex   = i;
      pushBlock.sampleDelta = settings_.irradianceSampleDelta;

      vkCmdPushConstants(commandBuffer, irradiancePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

      vkCmdDraw(commandBuffer, 36, 1, 0, 0);

      vkCmdEndRenderPass(commandBuffer);
    }

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device_.device(), framebuffer, nullptr);
    }
    for (auto imageView : imageViews)
    {
      vkDestroyImageView(device_.device(), imageView, nullptr);
    }

    // Transition to shader read
    transitionImageLayoutHelper(device_, irradianceImage_, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);
  }

  void IBLSystem::createPrefilterResources()
  {
    // Similar to Irradiance but different format and shader
    VkAttachmentDescription attachment{};
    attachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &attachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device_.device(), &renderPassInfo, nullptr, &prefilterRenderPass_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter render pass!");
    }

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &prefilterDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter descriptor set layout!");
    }

    // Pipeline Layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4) + sizeof(int) + sizeof(float) + sizeof(uint32_t); // ViewProj + FaceIndex + Roughness + SampleCount

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &prefilterDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &prefilterPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter pipeline layout!");
    }

    // Pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass                        = prefilterRenderPass_;
    pipelineConfig.pipelineLayout                    = prefilterPipelineLayout_;
    pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    auto vertCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(prefilter_envmap.vert.spv)");
    auto fragCode = Pipeline::readFile(std::string(SHADER_PATH) + R"(prefilter_envmap.frag.spv)");

    VkShaderModule vertModule;
    VkShaderModule fragModule;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &vertModule);

    createInfo.codeSize = fragCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &fragModule);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module              = vertModule;
    shaderStages[0].pName               = "main";
    shaderStages[0].flags               = 0;
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[0].pNext               = nullptr;

    shaderStages[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module              = fragModule;
    shaderStages[1].pName               = "main";
    shaderStages[1].flags               = 0;
    shaderStages[1].pSpecializationInfo = nullptr;
    shaderStages[1].pNext               = nullptr;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
    pipelineInfo.pViewportState      = &pipelineConfig.viewportInfo;
    pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
    pipelineInfo.pMultisampleState   = &pipelineConfig.multisampleInfo;
    pipelineInfo.pColorBlendState    = &pipelineConfig.colorBlendInfo;
    pipelineInfo.pDepthStencilState  = &pipelineConfig.depthStencilInfo;
    pipelineInfo.pDynamicState       = &pipelineConfig.dynamicStateInfo;
    pipelineInfo.layout              = prefilterPipelineLayout_;
    pipelineInfo.renderPass          = prefilterRenderPass_;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &prefilterPipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter pipeline!");
    }

    vkDestroyShaderModule(device_.device(), vertModule, nullptr);
    vkDestroyShaderModule(device_.device(), fragModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &prefilterDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create prefilter descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = prefilterDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &prefilterDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &prefilterDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate prefilter descriptor set!");
    }
  }

  void IBLSystem::generatePrefilteredEnvMap(Skybox& skybox)
  {
    // Update descriptor set
    VkDescriptorImageInfo const imageInfo = skybox.getDescriptorInfo();
    VkWriteDescriptorSet        descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = prefilterDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    glm::mat4 const captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 const captureViews[]    = {glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                         glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkImageView>   imageViews;

    for (int mip = 0; mip < settings_.prefilterMipLevels; ++mip)
    {
      auto const     baseSize  = static_cast<uint32_t>(settings_.prefilterSize);
      uint32_t const divisor   = 1u << static_cast<uint32_t>(mip);
      uint32_t const mipWidth  = (std::max)(1u, baseSize / divisor);
      uint32_t const mipHeight = (std::max)(1u, baseSize / divisor);
      float const    roughness = (float)mip / (float)(settings_.prefilterMipLevels - 1);

      for (int i = 0; i < 6; ++i)
      {
        VkImageView           faceView;
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = prefilteredImage_;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = mip;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount     = 1;

        vkCreateImageView(device_.device(), &viewInfo, nullptr, &faceView);
        imageViews.push_back(faceView);

        VkFramebuffer           framebuffer;
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = prefilterRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = &faceView;
        framebufferInfo.width           = mipWidth;
        framebufferInfo.height          = mipHeight;
        framebufferInfo.layers          = 1;

        vkCreateFramebuffer(device_.device(), &framebufferInfo, nullptr, &framebuffer);
        framebuffers.push_back(framebuffer);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = prefilterRenderPass_;
        renderPassInfo.framebuffer       = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {mipWidth, mipHeight};

        VkClearValue const clearValue  = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues    = &clearValue;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(mipWidth);
        viewport.height   = static_cast<float>(mipHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {mipWidth, mipHeight};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterPipelineLayout_, 0, 1, &prefilterDescSet_, 0, nullptr);

        struct PushBlock
        {
          glm::mat4 mvp;
          int       faceIndex;
          float     roughness;
          uint32_t  sampleCount;
        } pushBlock;

        pushBlock.mvp         = captureProjection * captureViews[i];
        pushBlock.faceIndex   = i;
        pushBlock.roughness   = roughness;
        pushBlock.sampleCount = settings_.prefilterSampleCount;

        vkCmdPushConstants(commandBuffer, prefilterPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

        vkCmdDraw(commandBuffer, 36, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
      }
    }

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device_.device(), framebuffer, nullptr);
    }
    for (auto imageView : imageViews)
    {
      vkDestroyImageView(device_.device(), imageView, nullptr);
    }

    transitionImageLayoutHelper(device_,
                                prefilteredImage_,
                                VK_FORMAT_R16G16B16A16_SFLOAT,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                settings_.prefilterMipLevels,
                                6);
  }

  void IBLSystem::createBRDFResources()
  {
    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &brdfDescSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF descriptor set layout!");
    }

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &brdfDescSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &brdfPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF pipeline layout!");
    }

    // Compute Pipeline
    auto compCode = Pipeline::readFile(std::string(SHADER_PATH) + "brdf_lut.comp.spv");

    VkShaderModule           compModule;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = compCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(compCode.data());
    vkCreateShaderModule(device_.device(), &createInfo, nullptr, &compModule);

    VkPipelineShaderStageCreateInfo shaderStage{};
    shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = compModule;
    shaderStage.pName  = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = shaderStage;
    pipelineInfo.layout = brdfPipelineLayout_;

    if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &brdfPipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF compute pipeline!");
    }

    vkDestroyShaderModule(device_.device(), compModule, nullptr);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &brdfDescPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create BRDF descriptor pool!");
    }

    // Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = brdfDescPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &brdfDescSetLayout_;

    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &brdfDescSet_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to allocate BRDF descriptor set!");
    }
  }

  void IBLSystem::generateBRDFLUT()
  {
    // Update descriptor set
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView   = brdfLUTImageView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = brdfDescSet_;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipelineLayout_, 0, 1, &brdfDescSet_, 0, nullptr);

    vkCmdDispatch(commandBuffer, settings_.brdfLUTSize / 16, settings_.brdfLUTSize / 16, 1);

    device_.getMemory().endSingleTimeCommands(commandBuffer);

    transitionImageLayoutHelper(device_, brdfLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  }

} // namespace engine
