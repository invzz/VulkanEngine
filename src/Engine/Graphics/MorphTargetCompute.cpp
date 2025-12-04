#include "Engine/Graphics/MorphTargetCompute.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/ansi_colors.hpp"

namespace engine {

  MorphTargetCompute::MorphTargetCompute(Device& device) : device_(device)
  {
    createDescriptorSetLayout();
    createComputePipeline();
    createDescriptorPool();

    std::cout << "[" << GREEN << "MorphTargetCompute" << RESET << "] Compute pipeline created" << std::endl;
  }

  MorphTargetCompute::~MorphTargetCompute()
  {
    vkDestroyPipeline(device_.device(), computePipeline_, nullptr);
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    // descriptorSetLayout_ and descriptorPool_ will be destroyed automatically
  }

  void MorphTargetCompute::createDescriptorSetLayout()
  {
    descriptorSetLayout_ = DescriptorSetLayout::Builder(device_)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // Base vertices
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // Morph deltas
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // Weights
                                   .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // Output
                                   .build();
  }

  void MorphTargetCompute::createComputePipeline()
  {
    // Read compiled compute shader
    std::string shaderPath = std::string(SHADER_PATH) + "/morph_blend.comp.spv";
    std::cout << "[MorphTargetCompute] Loading shader from: " << shaderPath << std::endl;
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
      std::cerr << "[MorphTargetCompute] Failed to open shader file: " << shaderPath << std::endl;
      throw ReadFileException(std::string("Failed to open compute shader: " + shaderPath).c_str());
    }

    auto              fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> shaderCode(fileSize);

    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    // Create shader module
    VkShaderModule computeShaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeShaderModule,
            .pName  = "main",
    };

    // Push constants for configuration
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = sizeof(PushConstants),
    };

    // Pipeline layout
    VkDescriptorSetLayout      layout = descriptorSetLayout_->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
    };

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderStageInfo,
            .layout = pipelineLayout_,
    };

    if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
  }

  void MorphTargetCompute::createDescriptorPool()
  {
    descriptorPool_ = DescriptorPool::Builder(device_)
                              .setMaxSets(25)
                              .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100)
                              .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                              .build();
  }

  VkShaderModule MorphTargetCompute::createShaderModule(const std::vector<char>& code)
  {
    VkShaderModuleCreateInfo createInfo{};

    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();

    // Safely convert std::vector<char> to std::vector<uint32_t>
    std::vector<uint32_t> codeAligned((code.size() + 3) / 4);
    std::memcpy(codeAligned.data(), code.data(), code.size());
    createInfo.pCode = codeAligned.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
      throw ShaderModuleCreationException("Failed to create compute shader module!");
    }

    return shaderModule;
  }

  VkDescriptorSet MorphTargetCompute::blend(VkCommandBuffer      commandBuffer,
                                            VkDescriptorSet      descriptorSet,
                                            VkBuffer             baseVertexBuffer,
                                            VkBuffer             morphDeltaBuffer,
                                            VkBuffer             weightsBuffer,
                                            VkBuffer             outputVertexBuffer,
                                            const PushConstants& pushConstants)
  {
    bool needsUpdate = false;

    // Allocate descriptor set if not provided
    if (descriptorSet == VK_NULL_HANDLE)
    {
      if (!descriptorPool_->allocateDescriptor(descriptorSetLayout_->getDescriptorSetLayout(), descriptorSet))
      {
        throw std::runtime_error("Failed to allocate compute descriptor set!");
      }

      needsUpdate = true;
    }

    // Update descriptor set with buffer bindings (only when first allocated)
    if (needsUpdate)
    {
      VkDescriptorBufferInfo baseVertexInfo{.buffer = baseVertexBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo morphDeltaInfo{.buffer = morphDeltaBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo weightsInfo{.buffer = weightsBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo outputInfo{.buffer = outputVertexBuffer, .offset = 0, .range = VK_WHOLE_SIZE};

      DescriptorWriter(*descriptorSetLayout_, *descriptorPool_)
              .writeBuffer(0, &baseVertexInfo)
              .writeBuffer(1, &morphDeltaInfo)
              .writeBuffer(2, &weightsInfo)
              .writeBuffer(3, &outputInfo)
              .overwrite(descriptorSet);
    }

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);

    // Push constants
    vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Dispatch compute shader
    // Work group size is 256, so divide vertex count by 256 and round up
    uint32_t workGroupCount = (pushConstants.vertexCount + 255) / 256;
    vkCmdDispatch(commandBuffer, workGroupCount, 1, 1);

    return descriptorSet;
  }

} // namespace engine
