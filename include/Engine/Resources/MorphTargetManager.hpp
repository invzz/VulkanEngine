#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/MorphTargetCompute.hpp"
#include "Engine/Resources/Model.hpp"

namespace engine {

  /**
   * @brief Manages GPU buffers and compute operations for morph target blending
   *
   * This class handles the creation of GPU buffers for morph target data and
   * orchestrates the compute shader execution to blend morph targets for animated models.
   */
  class MorphTargetManager
  {
  public:
    MorphTargetManager(Device& device);
    ~MorphTargetManager() = default;

    MorphTargetManager(const MorphTargetManager&)            = delete;
    MorphTargetManager& operator=(const MorphTargetManager&) = delete;

    /**
     * @brief Initialize GPU buffers for a model's morph targets
     * @param model The model containing morph target data
     * @return Model ID for future updates
     */
    void initializeModel(std::shared_ptr<Model> model);

    /**
     * @brief Update morph target weights for a model and dispatch compute shader
     * @param commandBuffer Vulkan command buffer
     * @param model The model to update
     */
    void updateAndBlend(VkCommandBuffer commandBuffer, std::shared_ptr<Model> model);

    /**
     * @brief Check if a model has been initialized for morph target blending
     */
    bool isModelInitialized(const Model* model) const;

    /**
     * @brief Get the blended vertex buffer for a model (to use for rendering)
     * @return VkBuffer handle or VK_NULL_HANDLE if not initialized
     */
    VkBuffer getBlendedBuffer(const Model* model) const;

    /**
     * @brief Get the device address of the blended vertex buffer
     * @return Device address or 0 if not initialized
     */
    uint64_t getBlendedBufferAddress(const Model* model) const;

  private:
    struct ModelMorphData
    {
      std::unique_ptr<Buffer> morphDeltaBuffer;               // Position and normal deltas
      std::unique_ptr<Buffer> weightsBuffer;                  // Current morph weights
      std::unique_ptr<Buffer> blendedBuffer;                  // Output blended vertices
      VkDescriptorSet         descriptorSet = VK_NULL_HANDLE; // Cached descriptor set
      size_t                  morphTargetCount;               // Number of morph targets
      size_t                  vertexCount;                    // Number of vertices
      uint32_t                vertexOffset;                   // Offset in vertex buffer
    };

    Device&                                          device_;
    std::unique_ptr<MorphTargetCompute>              compute_;
    std::unordered_map<const Model*, ModelMorphData> modelData_;

    void createMorphBuffers(const Model& model, ModelMorphData& data);
  };

} // namespace engine
