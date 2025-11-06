#include "3dEngine/Model.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <cassert>
#include <cstring>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <unordered_map>

#include "3dEngine/Model.hpp"
#include "3dEngine/ansi_colors.hpp"
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

  Model::Model(Device& device, const Builder& builder) : device{device}
  {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
  }

  std::unique_ptr<Model> Model::createModelFromFile(Device& device, const std::string& filepath)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading model from file: " << filepath << std::endl;
    Builder builder;
    builder.loadModelFromFile(std::string(MODEL_PATH) + filepath);
    std::cout << "[" << GREEN << "Model" << RESET << "]: " << filepath << " with " << builder.vertices.size() << " vertices " << std::endl;
    return std::make_unique<Model>(device, builder);
    return nullptr;
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

  void engine::Model::Builder::loadModelFromFile(const std::string& filepath)
  {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn;

    if (std::string err; !tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str()))
    {
      throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    vertices.clear();
    indices.clear();

    for (const auto& shape : shapes)
    {
      for (const auto& index : shape.mesh.indices)
      {
        Vertex vertex{};

        if (index.vertex_index >= 0)
        {
          vertex.position = {
                  attrib.vertices[3 * index.vertex_index + 0],
                  attrib.vertices[3 * index.vertex_index + 1],
                  attrib.vertices[3 * index.vertex_index + 2],
          };

          vertex.color = {
                  attrib.colors[3 * index.vertex_index + 0],
                  attrib.colors[3 * index.vertex_index + 1],
                  attrib.colors[3 * index.vertex_index + 2],
          };
        }

        else
        {
          vertex.color = {1.0f, 1.0f, 1.0f};
        }

        if (index.normal_index >= 0)
        {
          vertex.normal = {
                  attrib.normals[3 * index.normal_index + 0],
                  attrib.normals[3 * index.normal_index + 1],
                  attrib.normals[3 * index.normal_index + 2],
          };
        }
        if (index.texcoord_index >= 0)
        {
          vertex.uv = {
                  attrib.texcoords[2 * index.texcoord_index + 0],
                  attrib.texcoords[2 * index.texcoord_index + 1], // 1.0f - attrib.texcoords[2 * index.texcoord_index + 1] for flipping
          };
        }
        if (uniqueVertices.count(vertex) == 0)
        {
          uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
          vertices.push_back(vertex);
        }
        indices.push_back(uniqueVertices[vertex]);
      }
    }
  }

} // namespace engine
