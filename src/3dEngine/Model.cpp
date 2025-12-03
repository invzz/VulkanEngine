#include "3dEngine/Model.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <unordered_map>

#include "3dEngine/ansi_colors.hpp"
#include "3dEngine/importers/GLTFImporter.hpp"
#include "3dEngine/importers/OBJImporter.hpp"
#include "3dEngine/utils.hpp"

namespace std {
  template <> struct hash<engine::Model::Vertex>
  {
    size_t operator()(engine::Model::Vertex const& vertex) const
    {
      size_t seed = 0;
      engine::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
      return seed;
    }
  };
} // namespace std

namespace engine {

  Model::Model(Device& device, const Builder& builder)
      : device{device}, materials_{builder.materials}, subMeshes_{builder.subMeshes}, animations_{builder.animations}, nodes_{builder.nodes},
        morphTargetSets_{builder.morphTargetSets}, filePath{builder.filePath}
  {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
  }

  std::unique_ptr<Model> Model::createModelFromFile(Device& device, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading model from file: " << filepath << std::endl;
    Builder builder;
    builder.loadModelFromFile(filepath, flipX, flipY, flipZ);
    std::cout << "[" << GREEN << "Model" << RESET << "]: " << filepath << " with " << builder.vertices.size() << " vertices " << std::endl;
    return std::make_unique<Model>(device, builder);
    return nullptr;
  }

  Model::~Model() = default;

  void Model::bind(VkCommandBuffer commandBuffer) const
  {
    VkBuffer     buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer)
    {
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
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

  void Model::drawSubMesh(VkCommandBuffer commandBuffer, size_t subMeshIndex) const
  {
    if (subMeshIndex >= subMeshes_.size())
    {
      return;
    }

    const auto& subMesh = subMeshes_[subMeshIndex];

    if (hasIndexBuffer)
    {
      vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1, subMesh.indexOffset, 0, 0);
    }
    else
    {
      vkCmdDraw(commandBuffer, subMesh.indexCount, 1, subMesh.indexOffset, 0);
    }
  }

  void Model::bindAlternateVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer alternateVertexBuffer) const
  {
    VkBuffer     buffers[] = {alternateVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer)
    {
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
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
    uint32_t     indexSize  = sizeof(indices[0]);
    Buffer       stagingBuffer{device,
                         indexSize,
                         indexCount,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)indices.data(), bufferSize);

    indexBuffer = std::make_unique<Buffer>(device,
                                           indexSize,
                                           indexCount,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // copy data from staging buffer to index buffer
    device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                        indexBuffer->getBuffer(),
                                        bufferSize,
                                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                        VK_ACCESS_INDEX_READ_BIT);
  }

  void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
  {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
    uint32_t     vertexSize = sizeof(vertices[0]);

    Buffer stagingBuffer(device,
                         vertexSize,
                         vertexCount,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)vertices.data(), bufferSize);

    vertexBuffer = std::make_unique<Buffer>(device,
                                            vertexSize,
                                            vertexCount,
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // copy data from staging buffer to vertex buffer
    device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                        vertexBuffer->getBuffer(),
                                        bufferSize,
                                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
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
    attributeDescriptions.reserve(4);

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
    // Normal attribute
    attributeDescriptions.push_back({
            .location = 2,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, normal),
    });
    // UV attribute
    attributeDescriptions.push_back({
            .location = 3,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(Vertex, uv),
    });
    return attributeDescriptions;
  }

  void engine::Model::Builder::loadModelFromFile(const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    this->filePath = filepath;
    OBJImporter importer;
    if (!importer.load(*this, filepath, flipX, flipY, flipZ))
    {
      throw std::runtime_error("Failed to load OBJ file: " + filepath);
    }
  }

  std::unique_ptr<Model> Model::createModelFromGLTF(Device& device, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading glTF model from file: " << filepath << std::endl;
    Builder builder;
    builder.loadModelFromGLTF(filepath, flipX, flipY, flipZ);
    std::cout << "[" << GREEN << "Model" << RESET << "]: " << filepath << " with " << builder.vertices.size() << " vertices " << std::endl;
    return std::make_unique<Model>(device, builder);
  }

  void Model::Builder::loadModelFromGLTF(const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    this->filePath = filepath;
    GLTFImporter importer;
    if (!importer.load(*this, filepath, flipX, flipY, flipZ))
    {
      throw std::runtime_error("Failed to load glTF file: " + filepath);
    }
  }

  size_t Model::getMemorySize() const
  {
    size_t totalSize = 0;

    // Vertex buffer
    totalSize += vertexCount * sizeof(Vertex);

    // Index buffer
    if (hasIndexBuffer)
    {
      totalSize += indexCount * sizeof(uint32_t);
    }

    return totalSize;
  }

} // namespace engine
