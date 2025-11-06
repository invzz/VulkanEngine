#include "2dEngine/Model.hpp"

#include <cassert>
#include <cstring>

namespace engine {

  Model::Model(Device& device, const std::vector<Vertex>& vertices) : device{device}
  {
    createVertexBuffers(vertices);
  }

  Model::~Model()
  {
    // Clean up vertex buffer and memory
    vkDestroyBuffer(device.device(), vertexBuffer, nullptr);
    vkFreeMemory(device.device(), vertexBufferMemory, nullptr);
  }

  void Model::bind(VkCommandBuffer commandBuffer) const
  {
    VkBuffer     buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
  }

  void Model::draw(VkCommandBuffer commandBuffer) const
  {
    vkCmdDraw(commandBuffer, indexCount, 1, 0, 0);
  }

  void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
  {
    indexCount = static_cast<uint32_t>(vertices.size());
    assert(indexCount >= 3 && "Vertex count must be at least 3");

    VkDeviceSize bufferSize = sizeof(vertices[0]) * indexCount;

    device.memory().createBuffer(bufferSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 // accessible from CPU
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         // ensures data is immediately visible to GPU
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 vertexBuffer,
                                 vertexBufferMemory);

    void* data;
    vkMapMemory(device.device(), vertexBufferMemory, 0, bufferSize, 0, &data);
    // copy vertex data to mapped memory, no need to flush due to
    // HOST_COHERENT_BIT
    std::memcpy(data, vertices.data(), bufferSize);
    // unmap the memory after copying
    vkUnmapMemory(device.device(), vertexBufferMemory);
  }

  std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions()
  {
    return {
            {
                    .binding   = 0,
                    .stride    = sizeof(Vertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
    };
  }

  std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions()
  {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.reserve(2);

    // Position attribute
    attributeDescriptions.push_back({
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(Vertex, position),
    });
    // Color attribute
    attributeDescriptions.push_back({
            .location = 1,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, color),
    });

    return attributeDescriptions;
  }

} // namespace engine
