#include "3dEngine/Model.hpp"

#include <cassert>
#include <cstring>

namespace engine {

  Model::Model(Device& device, const Builder& builder) : device{device}
  {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
  }

  Model::~Model()
  {
    // Clean up vertex buffer and memory
    vkDestroyBuffer(device.device(), vertexBuffer, nullptr);
    vkFreeMemory(device.device(), vertexBufferMemory, nullptr);
    if (hasIndexBuffer)
    {
      vkDestroyBuffer(device.device(), indexBuffer, nullptr);
      vkFreeMemory(device.device(), indexBufferMemory, nullptr);
    }
  }

  void Model::bind(VkCommandBuffer commandBuffer) const
  {
    VkBuffer     buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer)
    {
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
  }

  void Model::draw(VkCommandBuffer commandBuffer) const
  {
    if (hasIndexBuffer)
    {
      vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
      return;
    }
    else
    {
      vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
  }

  void Model::createIndexBuffers(const std::vector<uint32_t>& indices)
  {
    indexCount     = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;
    if (!hasIndexBuffer)
    {
      return;
    }
    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // create staging buffer
    device.memory().createBuffer(bufferSize,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingBuffer,
                                 stagingBufferMemory);
    // map and copy index data to staging buffer
    void* data;
    vkMapMemory(device.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device.device(), stagingBufferMemory);

    // create index buffer
    device.memory().createBuffer(bufferSize,
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 indexBuffer,
                                 indexBufferMemory);

    // copy data from staging buffer to index buffer
    device.memory().copyBufferImmediate(stagingBuffer, indexBuffer, bufferSize, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT);

    // clean up staging buffer
    vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
    vkFreeMemory(device.device(), stagingBufferMemory, nullptr);
  }

  void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
  {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // create staging buffer
    device.memory().createBuffer(bufferSize,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingBuffer,
                                 stagingBufferMemory);
    // map and copy vertex data to staging buffer
    void* data;
    vkMapMemory(device.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(device.device(), stagingBufferMemory);

    // create vertex buffer
    device.memory().createBuffer(bufferSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 vertexBuffer,
                                 vertexBufferMemory);

    // copy data from staging buffer to vertex buffer
    device.memory().copyBufferImmediate(stagingBuffer, vertexBuffer, bufferSize, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

    // clean up staging buffer
    vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
    vkFreeMemory(device.device(), stagingBufferMemory, nullptr);
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
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
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
