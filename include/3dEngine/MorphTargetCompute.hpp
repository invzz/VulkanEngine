#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "Descriptors.hpp"
#include "Device.hpp"
#include "Model.hpp"

namespace engine {

  /**
   * @brief Manages GPU-side morph target blending using compute shaders
   *
   * This class handles the creation and execution of a compute pipeline that blends
   * morph targets (blend shapes) on the GPU. It takes base mesh vertices, morph target
   * deltas, and weights to produce blended output vertices.
   */
  class MorphTargetCompute
  {
  public:
    struct PushConstants
    {
      uint32_t vertexOffset;     // Offset into vertex buffer
      uint32_t vertexCount;      // Number of vertices to process
      uint32_t morphTargetCount; // Number of morph targets
      uint32_t deltaOffset;      // Offset into morph delta buffer
    };

    MorphTargetCompute(Device& device);
    ~MorphTargetCompute();

    MorphTargetCompute(const MorphTargetCompute&)            = delete;
    MorphTargetCompute& operator=(const MorphTargetCompute&) = delete;

    /**
     * @brief Execute morph target blending for a mesh
     * @param commandBuffer Vulkan command buffer to record commands into
     * @param descriptorSet Pre-allocated descriptor set (or VK_NULL_HANDLE to allocate new one)
     * @param baseVertexBuffer Buffer containing base mesh vertices
     * @param morphDeltaBuffer Buffer containing morph target deltas
     * @param weightsBuffer Buffer containing current morph weights
     * @param outputVertexBuffer Buffer where blended vertices will be written
     * @param pushConstants Configuration for this blend operation
     * @return The descriptor set used (for caching)
     */
    VkDescriptorSet blend(VkCommandBuffer      commandBuffer,
                          VkDescriptorSet      descriptorSet,
                          VkBuffer             baseVertexBuffer,
                          VkBuffer             morphDeltaBuffer,
                          VkBuffer             weightsBuffer,
                          VkBuffer             outputVertexBuffer,
                          const PushConstants& pushConstants);

  private:
    Device& device_;

    VkPipelineLayout                     pipelineLayout_;
    VkPipeline                           computePipeline_;
    std::unique_ptr<DescriptorSetLayout> descriptorSetLayout_;
    std::unique_ptr<DescriptorPool>      descriptorPool_;

    void           createDescriptorSetLayout();
    void           createComputePipeline();
    void           createDescriptorPool();
    VkShaderModule createShaderModule(const std::vector<char>& code);
  };

} // namespace engine
