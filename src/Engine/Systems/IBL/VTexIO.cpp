#include "Engine/Systems/IBL/VTexIO.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"

namespace engine::ibl_detail::vtex {

    namespace {
        bool readFileBytes(const std::string& filePath, Header& outHeader, std::vector<std::byte>& outData) {
            std::ifstream in(filePath, std::ios::binary);
            if (!in)
                return false;

            in.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
            if (!in)
                return false;

            if (outHeader.magic != 0x58455456 || outHeader.version != 1)
                return false;

            std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            outData.resize(bytes.size());
            if (!bytes.empty()) {
                std::memcpy(outData.data(), bytes.data(), bytes.size());
            }
            return true;
        }
    }  // namespace

    uint32_t bytesPerPixelFor(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return 8;
            case VK_FORMAT_R16G16_SFLOAT:
                return 4;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return 16;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:

                return 16;
            default:
                break;
        }
        throw std::runtime_error("Unsupported VTEX format for IBL assets");
    }

    bool writeImageFromRaw(const std::string& filePath, const void* data, size_t dataSize, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers) {
        if (data == nullptr)
            return false;

        uint32_t const bpp = bytesPerPixelFor(format);

        size_t expected = 0;
        for (uint32_t mip = 0; mip < mipLevels; ++mip) {
            uint32_t const w = (std::max) (1u, width >> mip);
            uint32_t const h = (std::max) (1u, height >> mip);
            expected += static_cast<size_t>(w) * static_cast<size_t>(h) * bpp * layers;
        }

        if (dataSize < expected)
            return false;

        Header header;
        header.vkFormat   = static_cast<uint32_t>(format);
        header.width      = width;
        header.height     = height;
        header.mipLevels  = mipLevels;
        header.layers     = layers;
        header.bytesPerPx = bpp;

        std::filesystem::path outPath(filePath);
        if (outPath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outPath.parent_path(), ec);
            if (ec) {
                return false;
            }
        }

        std::ofstream out(filePath, std::ios::binary);
        if (!out)
            return false;

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(expected));
        out.close();

        return static_cast<bool>(out);
    }

    bool writeImage(Device& device, const std::string& filePath, VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers) {
        if (image == VK_NULL_HANDLE)
            return false;

        uint32_t const bpp = bytesPerPixelFor(format);

        std::vector<VkBufferImageCopy> regions;
        regions.reserve(static_cast<size_t>(mipLevels) * static_cast<size_t>(layers));

        VkDeviceSize totalBytes = 0;
        for (uint32_t mip = 0; mip < mipLevels; ++mip) {
            uint32_t const     w        = (std::max) (1u, width >> mip);
            uint32_t const     h        = (std::max) (1u, height >> mip);
            VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * bpp;
            totalBytes += mipBytes * layers;
        }

        Buffer staging{device, 1, static_cast<uint32_t>(totalBytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

        VkDeviceSize offset = 0;
        for (uint32_t mip = 0; mip < mipLevels; ++mip) {
            uint32_t const     w        = (std::max) (1u, width >> mip);
            uint32_t const     h        = (std::max) (1u, height >> mip);
            VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * bpp;
            for (uint32_t layer = 0; layer < layers; ++layer) {
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

        ibl_detail::transitionImageLayout(device, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipLevels, layers);
        device.memory().copyImageToBuffer(image, staging.getBuffer(), regions);
        ibl_detail::transitionImageLayout(device, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, layers);

        staging.map();
        void* data = staging.getMappedMemory();

        Header header;
        header.vkFormat   = static_cast<uint32_t>(format);
        header.width      = width;
        header.height     = height;
        header.mipLevels  = mipLevels;
        header.layers     = layers;
        header.bytesPerPx = bpp;

        std::filesystem::path outPath(filePath);
        if (outPath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outPath.parent_path(), ec);
            if (ec) {
                staging.unmap();
                return false;
            }
        }

        std::ofstream out(filePath, std::ios::binary);
        if (!out) {
            staging.unmap();
            return false;
        }

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(totalBytes));
        out.close();

        staging.unmap();
        return static_cast<bool>(out);
    }

    bool writeCompressedImageFromRaw(const std::string& filePath,
        const void*                                     compressedData,
        size_t                                          compressedSize,
        VkFormat                                        compressedFormat,
        uint32_t                                        width,
        uint32_t                                        height,
        uint32_t                                        mipLevels,
        uint32_t                                        layers) {
        if (compressedData == nullptr)
            return false;

        uint32_t const bpp = bytesPerPixelFor(compressedFormat);

        Header header;
        header.vkFormat   = static_cast<uint32_t>(compressedFormat);
        header.width      = width;
        header.height     = height;
        header.mipLevels  = mipLevels;
        header.layers     = layers;
        header.bytesPerPx = bpp;

        std::filesystem::path outPath(filePath);
        if (outPath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outPath.parent_path(), ec);
            if (ec) {
                return false;
            }
        }

        std::ofstream out(filePath, std::ios::binary);
        if (!out)
            return false;

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        out.write(reinterpret_cast<const char*>(compressedData), static_cast<std::streamsize>(compressedSize));
        out.close();

        return static_cast<bool>(out);
    }

    bool readHeader(const std::string& filePath, Header& outHeader) {
        Header                 header{};
        std::vector<std::byte> data;
        if (!readFileBytes(filePath, header, data))
            return false;
        VkFormat const format = static_cast<VkFormat>(header.vkFormat);
        if (header.bytesPerPx != bytesPerPixelFor(format))
            return false;
        outHeader = header;
        return true;
    }

    bool loadImage(Device& device,
        const std::string& filePath,
        VkImage&           image,
        VkDeviceMemory&    memory,
        VkImageView&       view,
        VkSampler&         sampler,
        VkImageViewType    viewType,
        VkImageCreateFlags flags,
        Header*            outHeader) {
        Header                 header{};
        std::vector<std::byte> data;
        if (!readFileBytes(filePath, header, data))
            return false;

        VkFormat const format = static_cast<VkFormat>(header.vkFormat);
        if (header.bytesPerPx != bytesPerPixelFor(format))
            return false;

        if (sampler != VK_NULL_HANDLE) {
            VkSampler toDestroy = sampler;
            device.deferDestroy([toDestroy](VkDevice dev) { vkDestroySampler(dev, toDestroy, nullptr); });
            sampler = VK_NULL_HANDLE;
        }
        if (view != VK_NULL_HANDLE) {
            VkImageView toDestroy = view;
            device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyImageView(dev, toDestroy, nullptr); });
            view = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            VkImage toDestroy = image;
            device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyImage(dev, toDestroy, nullptr); });
            image = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            VkDeviceMemory toDestroy = memory;
            device.deferDestroy([toDestroy](VkDevice dev) { vkFreeMemory(dev, toDestroy, nullptr); });
            memory = VK_NULL_HANDLE;
        }

        ibl_detail::createImage(device,
            header.width,
            header.height,
            header.mipLevels,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image,
            memory,
            header.layers,
            flags);

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        view                      = ibl_detail::createImageView(device, image, format, aspect, header.mipLevels, viewType, 0, header.layers, 0);

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
        samplerInfo.maxLod                  = static_cast<float>(header.mipLevels);
        if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
            return false;

        Buffer staging{device, 1, static_cast<uint32_t>(data.size()), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        staging.map();
        staging.writeToBuffer((void*) data.data(), data.size());
        staging.unmap();

        std::vector<VkBufferImageCopy> regions;
        regions.reserve(static_cast<size_t>(header.mipLevels) * static_cast<size_t>(header.layers));

        VkDeviceSize offset = 0;
        for (uint32_t mip = 0; mip < header.mipLevels; ++mip) {
            uint32_t const     w        = (std::max) (1u, header.width >> mip);
            uint32_t const     h        = (std::max) (1u, header.height >> mip);
            VkDeviceSize const mipBytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * header.bytesPerPx;
            for (uint32_t layer = 0; layer < header.layers; ++layer) {
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

        ibl_detail::transitionImageLayout(device, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, header.mipLevels, header.layers);
        device.memory().copyBufferToImage(staging.getBuffer(), image, regions);
        ibl_detail::transitionImageLayout(device, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, header.mipLevels, header.layers);

        if (outHeader != nullptr) {
            *outHeader = header;
        }

        return true;
    }

}  // namespace engine::ibl_detail::vtex
