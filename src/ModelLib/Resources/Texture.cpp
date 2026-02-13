#include "ModelLib/Resources/Texture.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

// STB implementation provided by the dedicated 'stb_provider' target.
#include <stb_image.h>

// TinyEXR for EXR loading
#include <tinyexr.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

namespace engine {

Texture::Texture(Device& device, const std::string& filepath, bool srgb, bool flipY) : device_{device} {
  // Load image using stb_image
  int texChannels;

  if (flipY) {
    stbi_set_flip_vertically_on_load(1);
  }

  stbi_uc* pixels = stbi_load(filepath.c_str(), &width_, &height_, &texChannels, STBI_rgb_alpha);

  if (flipY) {
    stbi_set_flip_vertically_on_load(0);
  }

  if (pixels == nullptr) {
    throw std::runtime_error("Failed to load texture image: " + filepath);
  }

  VkDeviceSize const imageSize = width_ * height_ * 4;  // RGBA

  // Calculate mip levels
  mipLevels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(width_, height_)))) + 1;

  // Create staging buffer
  Buffer stagingBuffer{device_, 1, static_cast<uint32_t>(imageSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  stagingBuffer.map();
  stagingBuffer.writeToBuffer(pixels);
  stagingBuffer.unmap();

  stbi_image_free(pixels);

  // Choose format based on whether this is an sRGB texture (color) or linear
  // (data)
  VkFormat const format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
  format_ = format;

  // Create Vulkan image
  createImage(width_,
      height_,
      mipLevels_,
      format,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Transition image layout and copy buffer to image
  transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_);
  copyBufferToImage(stagingBuffer.getBuffer(), image_, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));

  // Generate mipmaps (this also transitions to SHADER_READ_ONLY_OPTIMAL)
  generateMipmaps(image_, format, width_, height_, mipLevels_);

  // Create image view and sampler
  createImageView(format);
  createSampler();

  std::cout << "[" << GREEN << "Texture" << RESET << "] Loaded: " << filepath << " (" << width_ << "x" << height_ << ", " << mipLevels_ << " mips)" << '\n';
}

Texture::~Texture() {
  // Defer destroys so resources aren't freed while still referenced by in-flight
  // command buffers or descriptor sets.
  VkSampler sampler = sampler_;
  VkImageView view = imageView_;
  VkImage image = image_;
  VkDeviceMemory mem = imageMemory_;

  if (sampler != VK_NULL_HANDLE) {
    device_.deferDestroy([sampler](VkDevice dev) { vkDestroySampler(dev, sampler, nullptr); });
  }
  if (view != VK_NULL_HANDLE) {
    device_.deferDestroy([view](VkDevice dev) { vkDestroyImageView(dev, view, nullptr); });
  }
  if (image != VK_NULL_HANDLE) {
    device_.deferDestroy([image](VkDevice dev) { vkDestroyImage(dev, image, nullptr); });
  }
  if (mem != VK_NULL_HANDLE) {
    device_.deferDestroy([mem](VkDevice dev) { vkFreeMemory(dev, mem, nullptr); });
  }
}

// Private constructor for creating textures from memory
Texture::Texture(Device& device, const unsigned char* pixels, int width, int height, VkFormat format) : device_{device}, width_{width}, height_{height} {
  format_ = format;
  VkDeviceSize const imageSize = width_ * height_ * 4;  // RGBA
  mipLevels_ = 1;                                       // No mipmaps for default textures

  // Create staging buffer
  Buffer stagingBuffer{device_, 1, static_cast<uint32_t>(imageSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void*)pixels);
  stagingBuffer.unmap();

  // Create Vulkan image
  createImage(width_,
      height_,
      mipLevels_,
      format,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Transition image layout and copy buffer to image
  transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_);
  copyBufferToImage(stagingBuffer.getBuffer(), image_, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));
  transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels_);

  // Create image view and sampler
  createImageView(format);
  createSampler();
}

std::shared_ptr<Texture> Texture::createWhiteTexture(Device& device) {
  unsigned char whitePixel[4] = {255, 255, 255, 255};
  return std::shared_ptr<Texture>(new Texture(device, whitePixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM));
}

std::shared_ptr<Texture> Texture::createNormalTexture(Device& device) {
  // Flat normal pointing up in tangent space: (0, 0, 1) -> (128, 128, 255) in
  // RGB
  unsigned char normalPixel[4] = {128, 128, 255, 255};
  return std::shared_ptr<Texture>(new Texture(device, normalPixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM));
}

// Load EXR file as a linear float RGBA texture
std::shared_ptr<Texture> Texture::createFromEXR(Device& device, const std::string& filepath) {
  const char* err = nullptr;
  int width = 0;
  int height = 0;
  float* rgba = nullptr;

  int ret = LoadEXR(&rgba, &width, &height, filepath.c_str(), &err);
  if (ret != TINYEXR_SUCCESS) {
    std::string msg = "Failed to load EXR: ";
    if (err != nullptr) {
      msg += err;
      FreeEXRErrorMessage(err);
    }
    throw std::runtime_error(msg);
  }

  if (rgba == nullptr || width <= 0 || height <= 0) {
    if (rgba != nullptr) free(rgba);
    throw std::runtime_error("Invalid EXR image data: " + filepath);
  }

  // Use the float-pixels constructor to create the texture
  std::shared_ptr<Texture> tex = std::shared_ptr<Texture>(new Texture(device, rgba, width, height, VK_FORMAT_R32G32B32A32_SFLOAT));

  // Free the temporary EXR data (staging buffer now contains the image)
  free(rgba);

  std::cout << "[Texture] Loaded EXR: " << filepath << " (" << width << "x" << height << ")" << '\n';

  return tex;
}

// CPU-only helper: load EXR into memory, write VTEX container to disk, and optionally load the VTEX into a Texture.
// Helper: convert float32 to IEEE754-16 (half) representation
static uint16_t floatToHalf(float f) {
  uint32_t x = *reinterpret_cast<uint32_t*>(&f);
  uint32_t sign = (x >> 16) & 0x8000u;
  uint32_t mant = x & 0x007fffffu;
  uint32_t exp = (x >> 23) & 0xffu;
  if (exp == 255) {
    // Inf or NaN
    if (mant != 0u) return static_cast<uint16_t>(sign | 0x7c01u);  // NaN
    return static_cast<uint16_t>(sign | 0x7c00u);                  // Inf
  }
  int32_t newexp = static_cast<int32_t>(exp) - 127 + 15;
  if (newexp >= 31) {
    // Overflow -> Inf
    return static_cast<uint16_t>(sign | 0x7c00u);
  }
  if (newexp <= 0) {
    // Subnormal or zero
    if (newexp < -10) return static_cast<uint16_t>(sign);
    uint32_t mant32 = mant | 0x00800000u;  // add implicit leading 1
    int32_t shift = 14 - newexp;
    uint32_t half_mant = (mant32 + (1u << (shift - 1))) >> shift;
    return static_cast<uint16_t>(sign | half_mant);
  }
  uint16_t res = static_cast<uint16_t>(sign | (static_cast<uint16_t>(newexp) << 10) | static_cast<uint16_t>(mant >> 13));
  return res;
}

std::shared_ptr<Texture> Texture::createFromEXR_CPUOnly(Device& device, const std::string& exrPath, const std::string& outVtexPath, bool loadIntoGpu, VkFormat targetFormat) {
  const char* err = nullptr;
  int width = 0;
  int height = 0;
  float* rgba = nullptr;

  int ret = LoadEXR(&rgba, &width, &height, exrPath.c_str(), &err);
  if (ret != TINYEXR_SUCCESS) {
    std::string msg = "Failed to load EXR: ";
    if (err != nullptr) {
      msg += err;
      FreeEXRErrorMessage(err);
    }
    throw std::runtime_error(msg);
  }

  if (rgba == nullptr || width <= 0 || height <= 0) {
    if (rgba != nullptr) free(rgba);
    throw std::runtime_error("Invalid EXR image data: " + exrPath);
  }

  bool ok = false;

  if (targetFormat == VK_FORMAT_R32G32B32A32_SFLOAT) {
    // Write a VTEX container from the raw float pixels on CPU (native EXR)
    size_t dataSize = static_cast<size_t>(sizeof(float) * 4 * width * height);
    ok = ibl_detail::vtex::writeImageFromRaw(outVtexPath, rgba, dataSize, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1, 1);
  } else if (targetFormat == VK_FORMAT_R16G16B16A16_SFLOAT) {
    // Convert float32 RGBA to float16 RGBA and write
    size_t count = static_cast<size_t>(4) * static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint16_t> halfs;
    halfs.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      halfs.push_back(floatToHalf(rgba[i]));
    }
    ok = ibl_detail::vtex::writeImageFromRaw(outVtexPath,
        halfs.data(),
        halfs.size() * sizeof(uint16_t),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1,
        1);
  } else if (targetFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK || targetFormat == VK_FORMAT_BC6H_SFLOAT_BLOCK) {
    // Use external Compressonator CLI if available to produce BC6H compressed DDS from EXR.
    // Expected CLI: CompressonatorCLI -fd BC6H <in.exr> <out.dds>
    const char* envCli = std::getenv("COMPRESSONATOR_CLI");
    std::string cliPath = (envCli != nullptr) ? envCli : std::string();
#ifdef COMPRESSONATOR_CLI
    if (cliPath.empty()) {
      // Prefer the compile-time known location if present
      cliPath = std::string(COMPRESSONATOR_CLI);
    }
#endif
    if (cliPath.empty()) {
      // check common locations in repo or installed paths
      if (std::filesystem::exists("tools/Compressonator/CompressonatorCLI")) {
        cliPath = "tools/Compressonator/CompressonatorCLI";
      } else if (std::filesystem::exists("tools/Compressonator/compressonatorcli-bin")) {
        cliPath = "tools/Compressonator/compressonatorcli-bin";
      } else if (std::filesystem::exists("tools/Compressonator/compressonatorcli")) {
        cliPath = "tools/Compressonator/compressonatorcli";
      } else if (std::filesystem::exists("external/compressonator/CompressonatorCLI")) {
        cliPath = "external/compressonator/CompressonatorCLI";
      } else if (std::filesystem::exists("/usr/bin/compressonatorcli")) {
        cliPath = "/usr/bin/compressonatorcli";
      }
    }

    if (cliPath.empty()) {
      // Fall back gracefully to R16F conversion so CI remains deterministic
      std::cerr << "[Texture] BC6H requested but Compressonator CLI not found; falling back to R16F" << '\n';
      size_t count = static_cast<size_t>(4) * static_cast<size_t>(width) * static_cast<size_t>(height);
      std::vector<uint16_t> halfs;
      halfs.reserve(count);
      for (size_t i = 0; i < count; ++i) {
        halfs.push_back(floatToHalf(rgba[i]));
      }
      ok = ibl_detail::vtex::writeImageFromRaw(outVtexPath,
          halfs.data(),
          halfs.size() * sizeof(uint16_t),
          VK_FORMAT_R16G16B16A16_SFLOAT,
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height),
          1,
          1);
    } else {
      // Invoke CLI to produce DDS
      std::string tmpDds = outVtexPath + std::string(".tmp.dds");
      std::string cmd;
      // Quote paths in case of spaces
      cmd += '"' + cliPath + '"';
      cmd += " -fd BC6H ";
      cmd += '"' + exrPath + '"';
      cmd += ' ';
      cmd += '"' + tmpDds + '"';
      int sc = std::system(cmd.c_str());
      if (sc != 0 || !std::filesystem::exists(tmpDds)) {
        std::cerr << "[Texture] Compressonator CLI failed (rc=" << sc << "); falling back to R16F" << '\n';
        // fallback to r16f
        size_t count = static_cast<size_t>(4) * static_cast<size_t>(width) * static_cast<size_t>(height);
        std::vector<uint16_t> halfs;
        halfs.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          halfs.push_back(floatToHalf(rgba[i]));
        }
        ok = ibl_detail::vtex::writeImageFromRaw(outVtexPath,
            halfs.data(),
            halfs.size() * sizeof(uint16_t),
            VK_FORMAT_R16G16B16A16_SFLOAT,
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1,
            1);
      } else {
        // Read DDS payload as raw bytes and write compressed VTEX container
        std::ifstream in(tmpDds, std::ios::binary);
        if (!in) {
          free(rgba);
          throw std::runtime_error("Failed to open temporary DDS from Compressonator: " + tmpDds);
        }
        std::vector<char> ddsBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        ok = ibl_detail::vtex::writeCompressedImageFromRaw(outVtexPath, ddsBytes.data(), ddsBytes.size(), targetFormat, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1, 1);

        // cleanup
        std::error_code ec;
        std::filesystem::remove(tmpDds, ec);
      }
    }
  } else {
    // Unsupported target format at this time
    free(rgba);
    throw std::runtime_error("Requested VTEX target format not supported in CPU path");
  }

  // Free the EXR memory after writing
  free(rgba);

  if (!ok) {
    throw std::runtime_error("Failed to write VTEX from EXR: " + outVtexPath);
  }

  if (loadIntoGpu) {
    // Load back into GPU and return the Texture
    return createFromVTEX(device, outVtexPath);
  }

  // Caller requested only file output
  return nullptr;
}

// Create a Texture by loading a VTEX container into a Vulkan image and adopting the handles
std::shared_ptr<Texture> Texture::createFromVTEX(Device& device, const std::string& filepath) {
  return createFromVTEX(device, filepath, false);
}

std::shared_ptr<Texture> Texture::createFromVTEX(Device& device, const std::string& filepath, bool cpuOnly) {
  ibl_detail::vtex::Header header{};

  if (cpuOnly) {
    if (!ibl_detail::vtex::readHeader(filepath, header)) {
      throw std::runtime_error("Failed to read VTEX header: " + filepath);
    }
    // Create a Texture instance that only contains metadata (no GPU resources)
    std::shared_ptr<Texture> tex =
        std::shared_ptr<Texture>(new Texture(device, static_cast<int>(header.width), static_cast<int>(header.height), header.mipLevels, static_cast<VkFormat>(header.vkFormat), true));
    std::cout << "[Texture] (cpu-only) Read VTEX: " << filepath << " (" << header.width << "x" << header.height << ")" << '\n';
    return tex;
  }

  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  // Use VTEX loader to populate image/memory/view/sampler
  bool ok = ibl_detail::vtex::loadImage(device, filepath, image, memory, view, sampler, VK_IMAGE_VIEW_TYPE_2D, 0, &header);
  if (!ok) {
    throw std::runtime_error("Failed to load VTEX: " + filepath);
  }

  // Adopt the handles into a new Texture instance
  std::shared_ptr<Texture> tex =
      std::shared_ptr<Texture>(new Texture(device, image, memory, view, sampler, (int)header.width, (int)header.height, header.mipLevels, static_cast<VkFormat>(header.vkFormat)));
  std::cout << "[Texture] Loaded VTEX: " << filepath << " (" << header.width << "x" << header.height << ")" << '\n';
  return tex;
}

Texture::Texture(Device& device, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler, int width, int height, uint32_t mipLevels, VkFormat format)
    : device_{device}, image_{image}, imageMemory_{memory}, imageView_{view}, sampler_{sampler}, width_{width}, height_{height}, mipLevels_{mipLevels}, format_{format} {
  // Constructor adopts externally-created Vulkan handles; nothing else to do
}

// CPU-only constructor: does not create GPU resources, used for test-only metadata verification
Texture::Texture(Device& device, int width, int height, uint32_t mipLevels, VkFormat format, bool cpuOnly) : device_{device}, width_{width}, height_{height}, mipLevels_{mipLevels}, cpuOnly_{cpuOnly} {
  format_ = format;
  image_ = VK_NULL_HANDLE;
  imageMemory_ = VK_NULL_HANDLE;
  imageView_ = VK_NULL_HANDLE;
  sampler_ = VK_NULL_HANDLE;
}

// Private constructor for creating textures from float RGBA memory
Texture::Texture(Device& device, const float* pixels, int width, int height, VkFormat format) : device_{device}, width_{width}, height_{height} {
  VkDeviceSize const imageSize = static_cast<VkDeviceSize>(sizeof(float) * 4 * width * height);
  mipLevels_ = 1;  // No mipmaps for EXR textures

  // Create staging buffer
  Buffer stagingBuffer{device_, 1, static_cast<uint32_t>(imageSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void*)pixels);
  stagingBuffer.unmap();

  // Create Vulkan image
  createImage(width_,
      height_,
      mipLevels_,
      format,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Transition image layout and copy buffer to image
  transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_);
  copyBufferToImage(stagingBuffer.getBuffer(), image_, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));
  transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels_);

  // Create image view and sampler
  createImageView(format);
  createSampler();
}

void Texture::createImage(int width, int height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = static_cast<uint32_t>(width);
  imageInfo.extent.height = static_cast<uint32_t>(height);
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(device_.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device_.device(), image_, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = device_.memory().findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate image memory!");
  }

  vkBindImageMemory(device_.device(), image_, imageMemory_, 0);
}

void Texture::createImageView(VkFormat format) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image_;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels_;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture image view!");
  }
}

void Texture::createSampler() {
  const VkPhysicalDeviceProperties& properties = device_.getProperties();

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(mipLevels_);

  if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture sampler!");
  }
}

void Texture::transitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
  VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("Unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  device_.memory().endSingleTimeCommands(commandBuffer);
}

void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  device_.memory().endSingleTimeCommands(commandBuffer);
}

void Texture::generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels) {
  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device_.getPhysicalDevice(), format, &formatProperties);

  if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0u) {
    throw std::runtime_error("Texture image format does not support linear blitting!");
  }

  VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = width;
  int32_t mipHeight = height;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  device_.memory().endSingleTimeCommands(commandBuffer);
}

size_t Texture::getMemorySize() const {
  // Calculate memory for base texture + all mipmaps
  // Format: RGBA8 (4 bytes per pixel) or sRGB8_A8 (also 4 bytes)
  size_t totalSize = 0;
  int w = width_;
  int h = height_;

  for (uint32_t level = 0; level < mipLevels_; ++level) {
    totalSize += static_cast<size_t>(w * h * 4);  // 4 bytes per pixel (RGBA8)
    w = std::max(1, w / 2);
    h = std::max(1, h / 2);
  }

  return totalSize;
}

}  // namespace engine
