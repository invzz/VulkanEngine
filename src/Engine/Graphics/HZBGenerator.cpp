#include "Engine/Graphics/HZBGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameBuffer.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  HZBGenerator::HZBGenerator(Device& device) : device_(device) {}

  HZBGenerator::~HZBGenerator()
  {
    destroyDescriptorPool();

    if (hzbPipeline_ != VK_NULL_HANDLE)
    {
      vkDestroyPipeline(device_.device(), hzbPipeline_, nullptr);
      hzbPipeline_ = VK_NULL_HANDLE;
    }
    if (hzbPipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), hzbPipelineLayout_, nullptr);
      hzbPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (hzbSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device_.device(), hzbSetLayout_, nullptr);
      hzbSetLayout_ = VK_NULL_HANDLE;
    }
  }

  void HZBGenerator::destroyDescriptorPool()
  {
    if (hzbDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device_.device(), hzbDescriptorPool_, nullptr);
      hzbDescriptorPool_ = VK_NULL_HANDLE;
    }
    hzbDescriptorSets_.clear();
    framesInFlight_ = 0;
    mipLevels_      = 0;
  }

  uint32_t HZBGenerator::calculateMipLevels(VkExtent2D extent)
  {
    if (extent.width == 0 || extent.height == 0)
    {
      return 0;
    }
    const auto maxDim = static_cast<uint32_t>(std::max(extent.width, extent.height));
    return static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(maxDim)))) + 1;
  }

  void HZBGenerator::createPipelineIfNeeded()
  {
    if (hzbSetLayout_ == VK_NULL_HANDLE)
    {
      VkDescriptorSetLayoutBinding bindings[2] = {};

      bindings[0].binding         = 0;
      bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[0].descriptorCount = 1;
      bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

      bindings[1].binding         = 1;
      bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      bindings[1].descriptorCount = 1;
      bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

      VkDescriptorSetLayoutCreateInfo layoutInfo{};
      layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      layoutInfo.bindingCount = 2;
      layoutInfo.pBindings    = bindings;

      if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &hzbSetLayout_) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB descriptor set layout!");
      }
    }

    if (hzbPipeline_ != VK_NULL_HANDLE)
    {
      return;
    }

#ifdef SHADER_PATH
    const std::string shaderPath = std::string(SHADER_PATH) + "/hiz_generate.comp.spv";
#else
    const std::string shaderPath = "assets/shaders/compiled/hiz_generate.comp.spv";
#endif

    const auto computeShaderCode = Pipeline::readFile(shaderPath);

    VkShaderModule computeShaderModule{VK_NULL_HANDLE};

    VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = computeShaderCode.size();
    shaderCreateInfo.pCode    = reinterpret_cast<const uint32_t*>(computeShaderCode.data());

    if (vkCreateShaderModule(device_.device(), &shaderCreateInfo, nullptr, &computeShaderModule) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create HZB shader module!");
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName  = "main";

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &hzbSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &hzbPipelineLayout_) != VK_SUCCESS)
    {
      vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
      throw std::runtime_error("failed to create HZB pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = hzbPipelineLayout_;
    pipelineInfo.stage  = shaderStageInfo;

    if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hzbPipeline_) != VK_SUCCESS)
    {
      vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
      throw std::runtime_error("failed to create HZB compute pipeline!");
    }

    vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
  }

  void HZBGenerator::recreateDescriptors(FrameBuffer& frameBuffer, VkExtent2D extent, uint32_t framesInFlight)
  {
    createPipelineIfNeeded();

    const uint32_t mipLevels = calculateMipLevels(extent);
    if (mipLevels == 0 || framesInFlight == 0)
    {
      destroyDescriptorPool();
      return;
    }

    if (hzbDescriptorPool_ != VK_NULL_HANDLE && mipLevels_ == mipLevels && framesInFlight_ == framesInFlight)
    {
      return;
    }

    destroyDescriptorPool();
    mipLevels_      = mipLevels;
    framesInFlight_ = framesInFlight;

    const uint32_t setsPerFrame = mipLevels_;
    const uint32_t totalSets    = setsPerFrame * framesInFlight_;

    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type                 = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount      = totalSets;
    poolSizes[1].type                 = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount      = totalSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = totalSets;

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &hzbDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create HZB descriptor pool!");
    }

    hzbDescriptorSets_.resize(framesInFlight_);
    for (uint32_t frame = 0; frame < framesInFlight_; frame++)
    {
      hzbDescriptorSets_[frame].resize(setsPerFrame);

      std::vector<VkDescriptorSetLayout> layouts(setsPerFrame, hzbSetLayout_);
      VkDescriptorSetAllocateInfo        allocInfo{};
      allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool     = hzbDescriptorPool_;
      allocInfo.descriptorSetCount = setsPerFrame;
      allocInfo.pSetLayouts        = layouts.data();

      const VkResult allocResult = vkAllocateDescriptorSets(device_.device(), &allocInfo, hzbDescriptorSets_[frame].data());
      if (allocResult != VK_SUCCESS)
      {
        throw std::runtime_error("failed to allocate HZB descriptor sets! Error: " + std::to_string(static_cast<int>(allocResult)));
      }

      for (uint32_t mip = 0; mip < setsPerFrame; mip++)
      {
        VkImageView inputView{VK_NULL_HANDLE};
        VkImageView outputView{VK_NULL_HANDLE};
        VkSampler   inputSampler{VK_NULL_HANDLE};

        if (mip == 0)
        {
          inputView    = frameBuffer.getDepthImageView(static_cast<int>(frame));
          outputView   = frameBuffer.getHzbMipImageView(static_cast<int>(frame), 0);
          inputSampler = frameBuffer.getDepthSampler();
        }
        else
        {
          inputView    = frameBuffer.getHzbMipImageView(static_cast<int>(frame), static_cast<int>(mip - 1));
          outputView   = frameBuffer.getHzbMipImageView(static_cast<int>(frame), static_cast<int>(mip));
          inputSampler = frameBuffer.getHzbSampler();
        }

        VkDescriptorImageInfo inputInfo{};
        inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inputInfo.imageView   = inputView;
        inputInfo.sampler     = inputSampler;

        VkDescriptorImageInfo outputInfo{};
        outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputInfo.imageView   = outputView;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet               = hzbDescriptorSets_[frame][mip];
        writes[0].dstBinding           = 0;
        writes[0].descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount      = 1;
        writes[0].pImageInfo           = &inputInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = hzbDescriptorSets_[frame][mip];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &outputInfo;

        vkUpdateDescriptorSets(device_.device(), 2, writes, 0, nullptr);
      }
    }
  }

  void HZBGenerator::generate(VkCommandBuffer commandBuffer, FrameBuffer& frameBuffer, VkExtent2D extent, int frameIndex)
  {
    if (frameIndex < 0)
    {
      return;
    }

    const uint32_t mipLevels = calculateMipLevels(extent);
    if (mipLevels < 2)
    {
      return;
    }

    if (framesInFlight_ == 0 || hzbDescriptorSets_.empty())
    {
      return;
    }

    if (mipLevels_ != mipLevels)
    {
      recreateDescriptors(frameBuffer, extent, framesInFlight_);
      if (mipLevels_ != mipLevels || hzbDescriptorSets_.empty())
      {
        return;
      }
    }

    if (static_cast<uint32_t>(frameIndex) >= hzbDescriptorSets_.size())
    {
      return;
    }

    VkImageMemoryBarrier hzbBarrier{};
    hzbBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hzbBarrier.image                           = frameBuffer.getHzbImage(frameIndex);
    hzbBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hzbBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hzbBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    hzbBarrier.subresourceRange.baseArrayLayer = 0;
    hzbBarrier.subresourceRange.layerCount     = 1;
    hzbBarrier.subresourceRange.baseMipLevel   = 0;
    hzbBarrier.subresourceRange.levelCount     = mipLevels_;
    hzbBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    hzbBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    hzbBarrier.srcAccessMask                   = 0;
    hzbBarrier.dstAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &hzbBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hzbPipeline_);

    const auto width  = static_cast<int32_t>(extent.width);
    const auto height = static_cast<int32_t>(extent.height);

    for (uint32_t mip = 0; mip < mipLevels_; mip++)
    {
      const int32_t currentWidth  = std::max(1, width >> static_cast<int32_t>(mip));
      const int32_t currentHeight = std::max(1, height >> static_cast<int32_t>(mip));

      const uint32_t mode = (mip == 0) ? 0u : 1u;
      vkCmdPushConstants(commandBuffer, hzbPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &mode);

      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hzbPipelineLayout_, 0, 1, &hzbDescriptorSets_[frameIndex][mip], 0, nullptr);

      vkCmdDispatch(commandBuffer, (currentWidth + 31) / 32, (currentHeight + 31) / 32, 1);

      VkImageMemoryBarrier mipBarrier{};
      mipBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      mipBarrier.image                           = frameBuffer.getHzbImage(frameIndex);
      mipBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      mipBarrier.subresourceRange.baseArrayLayer = 0;
      mipBarrier.subresourceRange.layerCount     = 1;
      mipBarrier.subresourceRange.baseMipLevel   = mip;
      mipBarrier.subresourceRange.levelCount     = 1;
      mipBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
      mipBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      mipBarrier.srcAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
      mipBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipBarrier);
    }
  }

} // namespace engine
