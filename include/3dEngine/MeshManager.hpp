#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Buffer.hpp"
#include "Device.hpp"
#include "Model.hpp"

namespace engine {

  class MeshManager
  {
  public:
    MeshManager(Device& device);
    ~MeshManager() = default;

    // Register a model and return its mesh ID
    uint32_t registerModel(const Model* model);

    // Get the descriptor info for the global mesh buffer
    VkDescriptorBufferInfo getDescriptorInfo() const;

    // Get the descriptor set layout binding for the mesh buffer
    static VkDescriptorSetLayoutBinding getDescriptorSetLayoutBinding();

  private:
    Device&                                    device;
    std::unique_ptr<Buffer>                    meshBuffer;
    std::vector<MeshBuffers>                   meshInfos;
    std::unordered_map<const Model*, uint32_t> modelToId;

    void updateBuffer();
  };

} // namespace engine
