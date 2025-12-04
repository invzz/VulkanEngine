#include "3dEngine/MorphTargetManager.hpp"

#include <iostream>

#include "3dEngine/ansi_colors.hpp"

namespace engine {

  // MorphDelta structure matching the shader (vec4 for 16-byte alignment)
  struct MorphDelta
  {
    glm::vec4 positionDelta; // w component unused but needed for alignment
    glm::vec4 normalDelta;   // w component unused but needed for alignment
  };

  MorphTargetManager::MorphTargetManager(Device& device) : device_(device)
  {
    compute_ = std::make_unique<MorphTargetCompute>(device_);
  }

  void MorphTargetManager::initializeModel(std::shared_ptr<Model> model)
  {
    if (!model || !model->hasMorphTargets())
    {
      return;
    }

    const Model* modelPtr = model.get();

    // Skip if already initialized
    if (modelData_.find(modelPtr) != modelData_.end())
    {
      return;
    }

    ModelMorphData data{};
    createMorphBuffers(*model, data);
    modelData_[modelPtr] = std::move(data);

    std::cout << "[" << GREEN << "MorphTargetManager" << RESET << "] Initialized model with " << data.morphTargetCount << " morph targets, " << data.vertexCount
              << " vertices" << std::endl;
  }

  void MorphTargetManager::createMorphBuffers(const Model& model, ModelMorphData& data)
  {
    const auto& morphTargetSets = model.getMorphTargetSets();

    if (morphTargetSets.empty())
    {
      return;
    }

    // For simplicity, handle the first morph target set
    // In a full implementation, you'd handle multiple sets
    const auto& morphSet = morphTargetSets[0];

    data.morphTargetCount = morphSet.targets.size();
    data.vertexCount      = morphSet.vertexCount;
    data.vertexOffset     = morphSet.vertexOffset;

    // Calculate buffer sizes
    size_t morphDeltaCount = data.morphTargetCount * data.vertexCount;
    size_t weightsCount    = data.morphTargetCount;

    // Create morph delta buffer (position and normal deltas)
    std::vector<MorphDelta> deltas;
    deltas.reserve(morphDeltaCount);

    for (const auto& target : morphSet.targets)
    {
      for (size_t i = 0; i < data.vertexCount; i++)
      {
        MorphDelta delta{};

        // Use position index mapping if available, otherwise direct indexing
        uint32_t posIdx = i;
        if (!morphSet.positionIndices.empty() && i < morphSet.positionIndices.size())
        {
          posIdx = morphSet.positionIndices[i];
        }

        delta.positionDelta = glm::vec4(target.positionDeltas[posIdx], 0.0f);
        delta.normalDelta   = glm::vec4(target.normalDeltas[posIdx], 0.0f);
        deltas.push_back(delta);
      }

      // Debug: verify mapping is working
      if (&target == &morphSet.targets[0])
      {
        std::cout << "[MorphTargetManager] Position index mapping sample: ";
        for (size_t i = 0; i < std::min(6ul, morphSet.positionIndices.size()); i++)
        {
          std::cout << i << "->" << morphSet.positionIndices[i] << " ";
        }
        std::cout << std::endl;
      }
    }

    data.morphDeltaBuffer = std::make_unique<Buffer>(device_,
                                                     sizeof(MorphDelta),
                                                     static_cast<uint32_t>(deltas.size()),
                                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Upload delta data
    Buffer stagingBuffer{device_,
                         sizeof(MorphDelta),
                         static_cast<uint32_t>(deltas.size()),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(deltas.data(), sizeof(MorphDelta) * deltas.size());
    stagingBuffer.unmap();

    device_.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                         data.morphDeltaBuffer->getBuffer(),
                                         sizeof(MorphDelta) * deltas.size(),
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         VK_ACCESS_SHADER_READ_BIT);

    // Create weights buffer (will be updated each frame)
    data.weightsBuffer = std::make_unique<Buffer>(device_,
                                                  sizeof(float),
                                                  static_cast<uint32_t>(weightsCount),
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    data.weightsBuffer->map();

    // Initialize with default weights
    std::vector<float> initialWeights(weightsCount, 0.0f);
    if (!morphSet.weights.empty())
    {
      for (size_t i = 0; i < std::min(weightsCount, morphSet.weights.size()); i++)
      {
        initialWeights[i] = morphSet.weights[i];
      }
    }
    data.weightsBuffer->writeToBuffer(initialWeights.data(), sizeof(float) * weightsCount);

    // Create blended output buffer (will store computed vertices)
    data.blendedBuffer =
            std::make_unique<Buffer>(device_,
                                     sizeof(Model::Vertex),
                                     static_cast<uint32_t>(data.vertexCount),
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }

  void MorphTargetManager::updateAndBlend(VkCommandBuffer commandBuffer, std::shared_ptr<Model> model)
  {
    if (!model || !model->hasMorphTargets())
    {
      return;
    }

    const Model* modelPtr = model.get();
    auto         it       = modelData_.find(modelPtr);

    if (it == modelData_.end())
    {
      // Not initialized, skip
      return;
    }

    auto& data = it->second;

    // Get current morph weights from model nodes
    // For simplicity, take weights from the first node that has them
    const auto&        nodes = model->getNodes();
    std::vector<float> currentWeights(data.morphTargetCount, 0.0f);

    for (const auto& node : nodes)
    {
      if (!node.morphWeights.empty())
      {
        static int debugFrameCount = 0;
        if (debugFrameCount++ < 5)
        { // Only print first 5 frames
          std::cout << "[MorphTargetManager] Frame weights: ";
          for (size_t i = 0; i < node.morphWeights.size(); i++)
          {
            std::cout << node.morphWeights[i] << " ";
          }
          std::cout << std::endl;
        }
        for (size_t i = 0; i < std::min(currentWeights.size(), node.morphWeights.size()); i++)
        {
          currentWeights[i] = node.morphWeights[i];
        }
        break; // Use the first node with weights
      }
    }

    // Update weights buffer
    data.weightsBuffer->writeToBuffer(currentWeights.data(), sizeof(float) * currentWeights.size());

    // Debug: print weights
    static int frameCount = 0;
    if (frameCount++ < 3)
    {
      std::cout << "[MorphTargetManager] Weights: ";
      for (size_t i = 0; i < currentWeights.size(); i++)
      {
        std::cout << currentWeights[i] << " ";
      }
      std::cout << std::endl;
    }

    // Setup push constants
    MorphTargetCompute::PushConstants pushConstants{
            .vertexOffset     = data.vertexOffset,
            .vertexCount      = static_cast<uint32_t>(data.vertexCount),
            .morphTargetCount = static_cast<uint32_t>(data.morphTargetCount),
            .deltaOffset      = 0,
    };

    // Debug: print push constants
    static bool printedOnce = false;
    if (!printedOnce)
    {
      std::cout << "[MorphTargetManager] Compute dispatch: offset=" << pushConstants.vertexOffset << " count=" << pushConstants.vertexCount
                << " morphTargets=" << pushConstants.morphTargetCount << std::endl;
      printedOnce = true;
    }

    // Dispatch compute shader and cache descriptor set
    data.descriptorSet = compute_->blend(commandBuffer,
                                         data.descriptorSet,
                                         model->getVertexBuffer(),
                                         data.morphDeltaBuffer->getBuffer(),
                                         data.weightsBuffer->getBuffer(),
                                         data.blendedBuffer->getBuffer(),
                                         pushConstants);

    // Add memory barrier between compute and graphics
    VkBufferMemoryBarrier barrier{
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = data.blendedBuffer->getBuffer(),
            .offset              = 0,
            .size                = VK_WHOLE_SIZE,
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
  }

  bool MorphTargetManager::isModelInitialized(const Model* model) const
  {
    return modelData_.find(model) != modelData_.end();
  }

  VkBuffer MorphTargetManager::getBlendedBuffer(const Model* model) const
  {
    auto it = modelData_.find(model);
    if (it == modelData_.end())
    {
      return VK_NULL_HANDLE;
    }
    return it->second.blendedBuffer->getBuffer();
  }

  uint64_t MorphTargetManager::getBlendedBufferAddress(const Model* model) const
  {
    auto it = modelData_.find(model);
    if (it == modelData_.end())
    {
      return 0;
    }
    return it->second.blendedBuffer->getDeviceAddress();
  }

} // namespace engine
