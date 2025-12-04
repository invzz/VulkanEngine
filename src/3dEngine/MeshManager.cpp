#include "3dEngine/MeshManager.hpp"

#include <iostream>

#include "3dEngine/ansi_colors.hpp"

namespace engine {

  MeshManager::MeshManager(Device& device) : device(device)
  {
    // Initialize with a dummy entry at index 0 so ID 0 can be "invalid" or "default"
    meshInfos.push_back({0, 0});
    updateBuffer();
  }

  uint32_t MeshManager::registerModel(const Model* model)
  {
    if (modelToId.find(model) != modelToId.end())
    {
      return modelToId[model];
    }

    uint32_t id = static_cast<uint32_t>(meshInfos.size());

    MeshBuffers info{};
    info.vertexBufferAddress = model->getVertexBufferAddress();
    info.indexBufferAddress  = model->getIndexBufferAddress();

    meshInfos.push_back(info);
    modelToId[model] = id;

    updateBuffer();

    std::cout << "[" << GREEN << "MeshManager" << RESET << "] Registered model with ID " << id << " (VA: " << info.vertexBufferAddress
              << ", IA: " << info.indexBufferAddress << ")" << std::endl;

    return id;
  }

  void MeshManager::updateBuffer()
  {
    VkDeviceSize bufferSize = sizeof(MeshBuffers) * meshInfos.size();

    // Create a staging buffer
    Buffer stagingBuffer{device,
                         sizeof(MeshBuffers),
                         static_cast<uint32_t>(meshInfos.size()),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(meshInfos.data());

    // Create or resize the GPU buffer
    // Note: In a real engine, you might want to allocate a larger buffer upfront to avoid frequent reallocations
    meshBuffer = std::make_unique<Buffer>(device,
                                          sizeof(MeshBuffers),
                                          static_cast<uint32_t>(meshInfos.size()),
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                        meshBuffer->getBuffer(),
                                        bufferSize,
                                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                        VK_ACCESS_SHADER_READ_BIT);
  }

  VkDescriptorBufferInfo MeshManager::getDescriptorInfo() const
  {
    return meshBuffer->descriptorInfo();
  }

  VkDescriptorSetLayoutBinding MeshManager::getDescriptorSetLayoutBinding()
  {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 1; // Binding 1 in Set 0
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    binding.pImmutableSamplers = nullptr;
    return binding;
  }

} // namespace engine
