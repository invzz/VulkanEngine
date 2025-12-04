#include "3dEngine/Model.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#define GLM_ENABLE_EXPERIMENTAL
#include <meshoptimizer.h>

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
    generateMeshlets(builder.vertices, builder.indices);
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
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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

  void Model::generateMeshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
  {
    if (indices.empty())
    {
      return;
    }

    const size_t max_vertices  = 64;
    const size_t max_triangles = 124;
    const float  cone_weight   = 0.0f;

    // Clear existing meshlets
    meshlets.clear();
    std::vector<unsigned int>  all_meshlet_vertices;
    std::vector<unsigned char> all_meshlet_triangles;

    // If no submeshes, create a default one
    if (subMeshes_.empty())
    {
      SubMesh sm{};
      sm.indexOffset = 0;
      sm.indexCount  = static_cast<uint32_t>(indices.size());
      sm.materialId  = 0;
      subMeshes_.push_back(sm);
    }

    for (auto& subMesh : subMeshes_)
    {
      size_t max_meshlets = meshopt_buildMeshletsBound(subMesh.indexCount, max_vertices, max_triangles);

      std::vector<meshopt_Meshlet> localMeshlets(max_meshlets);
      std::vector<unsigned int>    local_meshlet_vertices(max_meshlets * max_vertices);
      std::vector<unsigned char>   local_meshlet_triangles(max_meshlets * max_triangles * 3);

      size_t meshlet_count = meshopt_buildMeshlets(localMeshlets.data(),
                                                   local_meshlet_vertices.data(),
                                                   local_meshlet_triangles.data(),
                                                   &indices[subMesh.indexOffset],
                                                   subMesh.indexCount,
                                                   &vertices[0].position.x,
                                                   vertices.size(),
                                                   sizeof(Vertex),
                                                   max_vertices,
                                                   max_triangles,
                                                   cone_weight);

      const meshopt_Meshlet& last = localMeshlets[meshlet_count - 1];
      local_meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
      local_meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

      // Update SubMesh info
      subMesh.meshletOffset = static_cast<uint32_t>(meshlets.size());
      subMesh.meshletCount  = static_cast<uint32_t>(meshlet_count);

      // Offsets for this batch
      uint32_t vertexOffsetBase   = static_cast<uint32_t>(all_meshlet_vertices.size());
      uint32_t triangleOffsetBase = static_cast<uint32_t>(all_meshlet_triangles.size());

      // Append data
      all_meshlet_vertices.insert(all_meshlet_vertices.end(), local_meshlet_vertices.begin(), local_meshlet_vertices.end());
      all_meshlet_triangles.insert(all_meshlet_triangles.end(), local_meshlet_triangles.begin(), local_meshlet_triangles.end());

      for (size_t i = 0; i < meshlet_count; ++i)
      {
        const auto&    m      = localMeshlets[i];
        meshopt_Bounds bounds = meshopt_computeMeshletBounds(&local_meshlet_vertices[m.vertex_offset],
                                                             &local_meshlet_triangles[m.triangle_offset],
                                                             m.triangle_count,
                                                             &vertices[0].position.x,
                                                             vertices.size(),
                                                             sizeof(Vertex));

        Meshlet myMeshlet{};
        myMeshlet.vertexOffset   = m.vertex_offset + vertexOffsetBase;
        myMeshlet.triangleOffset = m.triangle_offset + triangleOffsetBase;
        myMeshlet.vertexCount    = m.vertex_count;
        myMeshlet.triangleCount  = m.triangle_count;

        memcpy(myMeshlet.center, bounds.center, sizeof(float) * 3);
        myMeshlet.radius = bounds.radius;
        memcpy(myMeshlet.cone_axis, bounds.cone_axis, sizeof(float) * 3);
        myMeshlet.cone_cutoff = bounds.cone_cutoff;

        meshlets.push_back(myMeshlet);
      }
    }

    // Create buffers
    // Meshlet Buffer
    {
      VkDeviceSize bufferSize = sizeof(Meshlet) * meshlets.size();
      Buffer       stagingBuffer{device,
                           sizeof(Meshlet),
                           static_cast<uint32_t>(meshlets.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(meshlets.data());

      meshletBuffer =
              std::make_unique<Buffer>(device,
                                       sizeof(Meshlet),
                                       static_cast<uint32_t>(meshlets.size()),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                          meshletBuffer->getBuffer(),
                                          bufferSize,
                                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                          VK_ACCESS_SHADER_READ_BIT);
    }

    // Meshlet Vertices Buffer
    {
      VkDeviceSize bufferSize = sizeof(unsigned int) * all_meshlet_vertices.size();
      Buffer       stagingBuffer{device,
                           sizeof(unsigned int),
                           static_cast<uint32_t>(all_meshlet_vertices.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(all_meshlet_vertices.data());

      meshletVerticesBuffer =
              std::make_unique<Buffer>(device,
                                       sizeof(unsigned int),
                                       static_cast<uint32_t>(all_meshlet_vertices.size()),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                          meshletVerticesBuffer->getBuffer(),
                                          bufferSize,
                                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                          VK_ACCESS_SHADER_READ_BIT);
    }

    // Meshlet Triangles Buffer
    {
      VkDeviceSize bufferSize = sizeof(unsigned char) * all_meshlet_triangles.size();

      Buffer stagingBuffer{device,
                           sizeof(unsigned char),
                           static_cast<uint32_t>(all_meshlet_triangles.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(all_meshlet_triangles.data());

      meshletTrianglesBuffer =
              std::make_unique<Buffer>(device,
                                       sizeof(unsigned char),
                                       static_cast<uint32_t>(all_meshlet_triangles.size()),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
                                          meshletTrianglesBuffer->getBuffer(),
                                          bufferSize,
                                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                          VK_ACCESS_SHADER_READ_BIT);
    }

    std::cout << "[" << GREEN << "Model" << RESET << "] Generated " << meshlets.size() << " meshlets." << std::endl;
  }

} // namespace engine
