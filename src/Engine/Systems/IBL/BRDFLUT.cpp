#include "Engine/Systems/IBL/BRDFLUT.hpp"

#include <stdexcept>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"

namespace engine::ibl {

    BRDFLUT::BRDFLUT(Device& device) : device_(device) {}

    BRDFLUT::~BRDFLUT() = default;

    void BRDFLUT::resetToUninitialized() {
        image_     = VK_NULL_HANDLE;
        memory_    = VK_NULL_HANDLE;
        imageView_ = VK_NULL_HANDLE;
        sampler_   = VK_NULL_HANDLE;

        currentSize_ = 0;

        pipelineLayout_ = VK_NULL_HANDLE;
        pipeline_       = VK_NULL_HANDLE;
        descSetLayout_  = VK_NULL_HANDLE;
        descPool_       = VK_NULL_HANDLE;
        descSet_        = VK_NULL_HANDLE;
    }

    VkDescriptorImageInfo BRDFLUT::getDescriptorInfo() const {
        return VkDescriptorImageInfo{
            .sampler     = sampler_,
            .imageView   = imageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    void BRDFLUT::adoptLoaded(VkImage image, VkDeviceMemory memory, VkImageView imageView, VkSampler sampler, int size) {
        image_       = image;
        memory_      = memory;
        imageView_   = imageView;
        sampler_     = sampler;
        currentSize_ = size;
    }

    void BRDFLUT::deferDestroyImageResources() {
        ibl_detail::deferDestroySampler(device_, sampler_);
        ibl_detail::deferDestroyImageView(device_, imageView_);
        ibl_detail::deferDestroyImage(device_, image_);
        ibl_detail::deferFreeMemory(device_, memory_);
        currentSize_ = 0;
    }

    void BRDFLUT::destroyImmediate() {
        VkDevice dev = device_.device();

        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(dev, sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }
        if (imageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(dev, image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(dev, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }

        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(dev, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }
        if (descPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, descPool_, nullptr);
            descPool_ = VK_NULL_HANDLE;
        }
        if (descSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, descSetLayout_, nullptr);
            descSetLayout_ = VK_NULL_HANDLE;
        }
        descSet_ = VK_NULL_HANDLE;

        currentSize_ = 0;
    }

    void BRDFLUT::createFallback() {
        ibl_detail::createImage(device_,
            1,
            1,
            1,
            VK_FORMAT_R16G16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image_,
            memory_);

        currentSize_ = 1;

        imageView_ = ibl_detail::createImageView(device_, image_, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D);

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

            if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
                throw std::runtime_error("failed to create fallback brdf LUT sampler!");
            }
        }

        VkClearColorValue const clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};

        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);

        VkCommandBuffer cmd = device_.getMemory().beginSingleTimeCommands();

        VkImageSubresourceRange range{};
        range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = 1;
        range.baseArrayLayer = 0;
        range.layerCount     = 1;

        vkCmdClearColorImage(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

        device_.getMemory().endSingleTimeCommands(cmd);

        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
    }

    void BRDFLUT::ensureForSettings(const Settings& settings) {
        if (image_ != VK_NULL_HANDLE && sampler_ != VK_NULL_HANDLE && imageView_ != VK_NULL_HANDLE && currentSize_ == settings.brdfLUTSize && settings.brdfLUTSize > 1) {
            return;
        }

        deferDestroyImageResources();

        createForSettings(settings);
        ensurePipelineResources();
        generate(settings);
    }

    void BRDFLUT::createForSettings(const Settings& settings) {
        ibl_detail::createImage(device_,
            settings.brdfLUTSize,
            settings.brdfLUTSize,
            1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image_,
            memory_);

        imageView_ = ibl_detail::createImageView(device_, image_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

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

        if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BRDF LUT sampler!");
        }

        currentSize_ = settings.brdfLUTSize;

        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
    }

    void BRDFLUT::ensurePipelineResources() {
        ibl_detail::deferDestroyPipeline(device_, pipeline_);
        ibl_detail::deferDestroyPipelineLayout(device_, pipelineLayout_);
        ibl_detail::deferDestroyDescriptorPool(device_, descPool_);
        ibl_detail::deferDestroyDescriptorSetLayout(device_, descSetLayout_);
        descSet_ = VK_NULL_HANDLE;

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

        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BRDF descriptor set layout!");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &descSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BRDF pipeline layout!");
        }

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
        pipelineInfo.layout = pipelineLayout_;

        if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BRDF compute pipeline!");
        }

        vkDestroyShaderModule(device_.device(), compModule, nullptr);

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 1;

        if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &descPool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BRDF descriptor pool!");
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descSetLayout_;

        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &descSet_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate BRDF descriptor set!");
        }
    }

    void BRDFLUT::generate(const Settings& settings) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView   = imageView_;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = descSet_;
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

        VkCommandBuffer commandBuffer = device_.getMemory().beginSingleTimeCommands();

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1, &descSet_, 0, nullptr);

        vkCmdDispatch(commandBuffer, settings.brdfLUTSize / 16, settings.brdfLUTSize / 16, 1);

        device_.getMemory().endSingleTimeCommands(commandBuffer);

        ibl_detail::transitionImageLayout(device_, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }

}  // namespace engine::ibl
