#include "Engine/Resources/Model.hpp"

#include <meshoptimizer.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Core/utils.hpp"
#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

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

  namespace {
    /**
     * @brief Compute axis-aligned bounding box for a set of triangle indices
     */
    AABB computeTriangleAABB(const std::vector<Model::Vertex>& vertices,
                             const std::vector<unsigned int>&  meshletVertices,
                             const std::vector<unsigned char>& meshletTriangles,
                             uint32_t                          triangleOffset,
                             uint32_t                          triangleCount,
                             uint32_t                          vertexOffset)
    {
      AABB box;
      for (uint32_t t = 0; t < triangleCount; ++t)
      {
        for (int v = 0; v < 3; ++v)
        {
          unsigned char localIdx  = meshletTriangles[triangleOffset + (t * 3) + v];
          unsigned int  vertexIdx = meshletVertices[vertexOffset + localIdx];
          box.expand(vertices[vertexIdx].position);
        }
      }
      return box;
    }

    /**
     * @brief Compute the centroid of a triangle
     */
    glm::vec3 computeTriangleCentroid(const std::vector<Model::Vertex>& vertices,
                                      const std::vector<unsigned int>&  meshletVertices,
                                      const std::vector<unsigned char>& meshletTriangles,
                                      uint32_t                          triangleOffset,
                                      uint32_t                          triangleIndex,
                                      uint32_t                          vertexOffset)
    {
      glm::vec3 sum{0.0f};
      for (int v = 0; v < 3; ++v)
      {
        unsigned char localIdx  = meshletTriangles[triangleOffset + (triangleIndex * 3) + v];
        unsigned int  vertexIdx = meshletVertices[vertexOffset + localIdx];
        sum += vertices[vertexIdx].position;
      }
      return sum / 3.0f;
    }

    /**
     * @brief Split a meshlet's triangles along the longest axis of the bounding box
     * @return Two vectors of triangle indices (relative to the original meshlet)
     */
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> splitTrianglesAlongLongestAxis(const std::vector<Model::Vertex>& vertices,
                                                                                           const std::vector<unsigned int>&  meshletVertices,
                                                                                           const std::vector<unsigned char>& meshletTriangles,
                                                                                           uint32_t                          triangleOffset,
                                                                                           uint32_t                          triangleCount,
                                                                                           uint32_t                          vertexOffset)
    {
      // Compute AABB to find the longest axis
      AABB box = computeTriangleAABB(vertices, meshletVertices, meshletTriangles, triangleOffset, triangleCount, vertexOffset);

      glm::vec3 extents   = box.extents();
      int       splitAxis = 0;
      if (extents.y > extents.x && extents.y > extents.z)
      {
        splitAxis = 1;
      }
      else if (extents.z > extents.x && extents.z > extents.y)
      {
        splitAxis = 2;
      }

      float splitPlane = box.center()[splitAxis];

      std::vector<uint32_t> leftTris;
      std::vector<uint32_t> rightTris;
      leftTris.reserve(triangleCount / 2);
      rightTris.reserve(triangleCount / 2);

      for (uint32_t t = 0; t < triangleCount; ++t)
      {
        glm::vec3 centroid = computeTriangleCentroid(vertices, meshletVertices, meshletTriangles, triangleOffset, t, vertexOffset);
        if (centroid[splitAxis] < splitPlane)
        {
          leftTris.push_back(t);
        }
        else
        {
          rightTris.push_back(t);
        }
      }

      // Handle edge case: all triangles on one side
      if (leftTris.empty() || rightTris.empty())
      {
        size_t mid = triangleCount / 2;
        leftTris.clear();
        rightTris.clear();
        for (uint32_t t = 0; t < triangleCount; ++t)
        {
          if (t < mid)
          {
            leftTris.push_back(t);
          }
          else
          {
            rightTris.push_back(t);
          }
        }
      }

      return {leftTris, rightTris};
    }

    /**
     * @brief Create a new meshlet from a subset of triangles of an existing meshlet
     * Returns the new meshlet data (vertices and triangles) and updates the Meshlet struct
     */
    void createMeshletFromTriangles(const std::vector<unsigned int>&  srcMeshletVertices,
                                    const std::vector<unsigned char>& srcMeshletTriangles,
                                    uint32_t                          srcVertexOffset,
                                    uint32_t                          srcTriangleOffset,
                                    const std::vector<uint32_t>&      triangleIndices,
                                    std::vector<unsigned int>&        outMeshletVertices,
                                    std::vector<unsigned char>&       outMeshletTriangles,
                                    Model::Meshlet&                   outMeshlet)
    {
      // Build a mapping from old local vertex indices to new local vertex indices
      std::unordered_map<unsigned char, unsigned char> vertexRemap;
      std::vector<unsigned int>                        newVertices;

      for (uint32_t triLocalIdx : triangleIndices)
      {
        for (int v = 0; v < 3; ++v)
        {
          unsigned char oldLocalIdx = srcMeshletTriangles[srcTriangleOffset + (triLocalIdx * 3) + v];
          if (!vertexRemap.contains(oldLocalIdx))
          {
            unsigned char newLocalIdx = static_cast<unsigned char>(newVertices.size());
            vertexRemap[oldLocalIdx]  = newLocalIdx;
            newVertices.push_back(srcMeshletVertices[srcVertexOffset + oldLocalIdx]);
          }
        }
      }

      // Build new triangle data
      std::vector<unsigned char> newTriangles;
      newTriangles.reserve(triangleIndices.size() * 3);
      for (uint32_t triLocalIdx : triangleIndices)
      {
        for (int v = 0; v < 3; ++v)
        {
          unsigned char oldLocalIdx = srcMeshletTriangles[srcTriangleOffset + (triLocalIdx * 3) + v];
          newTriangles.push_back(vertexRemap[oldLocalIdx]);
        }
      }

      // Pad triangles to 4-byte alignment
      while (newTriangles.size() % 4 != 0)
      {
        newTriangles.push_back(0);
      }

      // Fill output meshlet
      outMeshlet.vertexOffset   = static_cast<uint32_t>(outMeshletVertices.size());
      outMeshlet.triangleOffset = static_cast<uint32_t>(outMeshletTriangles.size());
      outMeshlet.vertexCount    = static_cast<uint32_t>(newVertices.size());
      outMeshlet.triangleCount  = static_cast<uint32_t>(triangleIndices.size());

      // Append to output arrays
      outMeshletVertices.insert(outMeshletVertices.end(), newVertices.begin(), newVertices.end());
      outMeshletTriangles.insert(outMeshletTriangles.end(), newTriangles.begin(), newTriangles.end());
    }

    /**
     * @brief Compute bounding sphere and cone for a meshlet
     */
    void computeMeshletBounds(const std::vector<Model::Vertex>& vertices, const std::vector<unsigned int>& meshletVertices, const std::vector<unsigned char>& meshletTriangles, Model::Meshlet& meshlet)
    {
      meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertexOffset],
                                                           &meshletTriangles[meshlet.triangleOffset],
                                                           meshlet.triangleCount,
                                                           &vertices[0].position.x,
                                                           vertices.size(),
                                                           sizeof(Model::Vertex));

      memcpy(meshlet.center, bounds.center, sizeof(float) * 3);
      meshlet.radius = bounds.radius;
      memcpy(meshlet.cone_axis, bounds.cone_axis, sizeof(float) * 3);
      meshlet.cone_cutoff = bounds.cone_cutoff;
    }

    /**
     * @brief Recursively split oversized meshlets until all meet the radius constraint
     */
    void splitOversizedMeshlets(const std::vector<Model::Vertex>&   vertices,
                                const std::vector<unsigned int>&    srcMeshletVertices,
                                const std::vector<unsigned char>&   srcMeshletTriangles,
                                const std::vector<meshopt_Meshlet>& srcMeshlets,
                                float                               maxRadius,
                                std::vector<unsigned int>&          outMeshletVertices,
                                std::vector<unsigned char>&         outMeshletTriangles,
                                std::vector<Model::Meshlet>&        outMeshlets)
    {
      struct WorkItem
      {
        std::vector<unsigned int>  meshletVertices;
        std::vector<unsigned char> meshletTriangles;
        uint32_t                   vertexCount;
        uint32_t                   triangleCount;
      };

      std::vector<WorkItem> workQueue;

      // Initialize work queue with original meshlets
      for (const auto& m : srcMeshlets)
      {
        WorkItem item;
        item.vertexCount   = m.vertex_count;
        item.triangleCount = m.triangle_count;

        // Copy vertex data
        item.meshletVertices.resize(m.vertex_count);
        for (uint32_t i = 0; i < m.vertex_count; ++i)
        {
          item.meshletVertices[i] = srcMeshletVertices[m.vertex_offset + i];
        }

        // Copy triangle data (including padding)
        size_t triDataSize = (m.triangle_count * 3 + 3) & ~3;
        item.meshletTriangles.resize(triDataSize);
        for (size_t i = 0; i < triDataSize; ++i)
        {
          item.meshletTriangles[i] = srcMeshletTriangles[m.triangle_offset + i];
        }

        workQueue.push_back(std::move(item));
      }

      while (!workQueue.empty())
      {
        WorkItem item = std::move(workQueue.back());
        workQueue.pop_back();

        // Create temporary meshlet to compute bounds
        Model::Meshlet tempMeshlet{};
        tempMeshlet.vertexOffset   = 0;
        tempMeshlet.triangleOffset = 0;
        tempMeshlet.vertexCount    = item.vertexCount;
        tempMeshlet.triangleCount  = item.triangleCount;

        computeMeshletBounds(vertices, item.meshletVertices, item.meshletTriangles, tempMeshlet);

        // Check if this meshlet needs splitting
        if (tempMeshlet.radius <= maxRadius || item.triangleCount <= 1)
        {
          // Meshlet is small enough or can't be split further
          Model::Meshlet finalMeshlet{};
          finalMeshlet.vertexOffset   = static_cast<uint32_t>(outMeshletVertices.size());
          finalMeshlet.triangleOffset = static_cast<uint32_t>(outMeshletTriangles.size());
          finalMeshlet.vertexCount    = item.vertexCount;
          finalMeshlet.triangleCount  = item.triangleCount;

          memcpy(finalMeshlet.center, tempMeshlet.center, sizeof(float) * 3);
          finalMeshlet.radius = tempMeshlet.radius;
          memcpy(finalMeshlet.cone_axis, tempMeshlet.cone_axis, sizeof(float) * 3);
          finalMeshlet.cone_cutoff = tempMeshlet.cone_cutoff;

          outMeshletVertices.insert(outMeshletVertices.end(), item.meshletVertices.begin(), item.meshletVertices.end());

          // Ensure triangles are padded to 4-byte alignment
          size_t triDataSize = (item.triangleCount * 3 + 3) & ~3;
          item.meshletTriangles.resize(triDataSize, 0);
          outMeshletTriangles.insert(outMeshletTriangles.end(), item.meshletTriangles.begin(), item.meshletTriangles.begin() + static_cast<std::ptrdiff_t>(triDataSize));

          outMeshlets.push_back(finalMeshlet);
        }
        else
        {
          // Split this meshlet
          auto [leftTris, rightTris] = splitTrianglesAlongLongestAxis(vertices, item.meshletVertices, item.meshletTriangles, 0, item.triangleCount, 0);

          // Create left work item
          if (!leftTris.empty())
          {
            WorkItem       leftItem;
            Model::Meshlet tempLeft{};
            createMeshletFromTriangles(item.meshletVertices, item.meshletTriangles, 0, 0, leftTris, leftItem.meshletVertices, leftItem.meshletTriangles, tempLeft);
            leftItem.vertexCount   = tempLeft.vertexCount;
            leftItem.triangleCount = tempLeft.triangleCount;
            workQueue.push_back(std::move(leftItem));
          }

          // Create right work item
          if (!rightTris.empty())
          {
            WorkItem       rightItem;
            Model::Meshlet tempRight{};
            createMeshletFromTriangles(item.meshletVertices, item.meshletTriangles, 0, 0, rightTris, rightItem.meshletVertices, rightItem.meshletTriangles, tempRight);
            rightItem.vertexCount   = tempRight.vertexCount;
            rightItem.triangleCount = tempRight.triangleCount;
            workQueue.push_back(std::move(rightItem));
          }
        }
      }
    }
  } // anonymous namespace

  // Initialize global meshlet build configuration with defaults
  Model::MeshletBuildConfig Model::s_meshletConfig_{};

  void Model::setMeshletBuildConfig(const MeshletBuildConfig& cfg)
  {
    s_meshletConfig_ = cfg;
  }

  const Model::MeshletBuildConfig& Model::getMeshletBuildConfig()
  {
    return s_meshletConfig_;
  }

  Model::Model(Device& device, const Builder& builder)
      : device{device}, materials_{builder.materials}, subMeshes_{builder.subMeshes}, animations_{builder.animations}, nodes_{builder.nodes}, morphTargetSets_{builder.morphTargetSets},
        filePath{builder.filePath}
  {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
    generateMeshlets(builder.vertices, builder.indices);
  }

  Model::~Model() = default;

  void Model::bind(VkCommandBuffer commandBuffer) const
  {
    VkBuffer const buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize   offsets[] = {0};
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

    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
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
    VkBuffer const buffers[] = {alternateVertexBuffer};
    VkDeviceSize   offsets[] = {0};
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
    VkDeviceSize const bufferSize = sizeof(indices[0]) * indexCount;
    uint32_t const     indexSize  = sizeof(indices[0]);
    Buffer             stagingBuffer{device, indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)indices.data(), bufferSize);

    indexBuffer = std::make_unique<Buffer>(device,
                                           indexSize,
                                           indexCount,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // copy data from staging buffer to index buffer
    device.memory().copyBufferImmediate(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT);
  }

  void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
  {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize const bufferSize = sizeof(vertices[0]) * vertexCount;
    uint32_t const     vertexSize = sizeof(vertices[0]);

    // Compute local-space AABB from vertex positions
    localBounds_ = AABB{};
    for (const auto& v : vertices)
    {
      localBounds_.expand(v.position);
    }

    Buffer stagingBuffer(device, vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)vertices.data(), bufferSize);

    vertexBuffer = std::make_unique<Buffer>(device,
                                            vertexSize,
                                            vertexCount,
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // copy data from staging buffer to vertex buffer
    device.memory().copyBufferImmediate(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
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

    const size_t max_vertices  = s_meshletConfig_.maxVertices;
    const size_t max_triangles = s_meshletConfig_.maxTriangles;
    const float  cone_weight   = s_meshletConfig_.coneWeight;
    const float  max_radius    = s_meshletConfig_.maxRadius;

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

    size_t originalMeshletCount = 0;
    size_t splitMeshletCount    = 0;

    for (auto& subMesh : subMeshes_)
    {
      size_t const max_meshlets = meshopt_buildMeshletsBound(subMesh.indexCount, max_vertices, max_triangles);

      std::vector<meshopt_Meshlet> localMeshlets(max_meshlets);
      std::vector<unsigned int>    local_meshlet_vertices(max_meshlets * max_vertices);
      std::vector<unsigned char>   local_meshlet_triangles(max_meshlets * max_triangles * 3);

      size_t const meshlet_count = meshopt_buildMeshlets(localMeshlets.data(),
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
      localMeshlets.resize(meshlet_count);
      local_meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
      local_meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

      originalMeshletCount += meshlet_count;

      // Update SubMesh info (will be adjusted after potential splitting)
      subMesh.meshletOffset = static_cast<uint32_t>(meshlets.size());

      // Check if we need to split oversized meshlets
      if (max_radius > 0.0f)
      {
        // Use the splitting algorithm
        std::vector<unsigned int>  split_meshlet_vertices;
        std::vector<unsigned char> split_meshlet_triangles;
        std::vector<Meshlet>       split_meshlets;

        splitOversizedMeshlets(vertices, local_meshlet_vertices, local_meshlet_triangles, localMeshlets, max_radius, split_meshlet_vertices, split_meshlet_triangles, split_meshlets);

        // Offsets for this batch
        uint32_t const vertexOffsetBase   = static_cast<uint32_t>(all_meshlet_vertices.size());
        uint32_t const triangleOffsetBase = static_cast<uint32_t>(all_meshlet_triangles.size());

        // Adjust offsets for the split meshlets and add to global list
        for (auto& m : split_meshlets)
        {
          m.vertexOffset += vertexOffsetBase;
          m.triangleOffset += triangleOffsetBase;
          meshlets.push_back(m);
        }

        // Append vertex and triangle data
        all_meshlet_vertices.insert(all_meshlet_vertices.end(), split_meshlet_vertices.begin(), split_meshlet_vertices.end());
        all_meshlet_triangles.insert(all_meshlet_triangles.end(), split_meshlet_triangles.begin(), split_meshlet_triangles.end());

        subMesh.meshletCount = static_cast<uint32_t>(split_meshlets.size());
        splitMeshletCount += split_meshlets.size();
      }
      else
      {
        // Original path without splitting
        // Offsets for this batch
        uint32_t const vertexOffsetBase   = static_cast<uint32_t>(all_meshlet_vertices.size());
        uint32_t const triangleOffsetBase = static_cast<uint32_t>(all_meshlet_triangles.size());

        // Append data
        all_meshlet_vertices.insert(all_meshlet_vertices.end(), local_meshlet_vertices.begin(), local_meshlet_vertices.end());
        all_meshlet_triangles.insert(all_meshlet_triangles.end(), local_meshlet_triangles.begin(), local_meshlet_triangles.end());

        for (auto i = 0; std::cmp_less(i, meshlet_count); ++i)
        {
          auto&          m      = localMeshlets[i];
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

        subMesh.meshletCount = static_cast<uint32_t>(meshlet_count);
      }
    }

    // Create buffers
    // Meshlet Buffer
    {
      VkDeviceSize const bufferSize = sizeof(Meshlet) * meshlets.size();
      Buffer             stagingBuffer{device,
                           sizeof(Meshlet),
                           static_cast<uint32_t>(meshlets.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(meshlets.data());

      meshletBuffer = std::make_unique<Buffer>(device,
                                               sizeof(Meshlet),
                                               static_cast<uint32_t>(meshlets.size()),
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(), meshletBuffer->getBuffer(), bufferSize, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    // Meshlet Vertices Buffer
    {
      VkDeviceSize const bufferSize = sizeof(unsigned int) * all_meshlet_vertices.size();
      Buffer             stagingBuffer{device,
                           sizeof(unsigned int),
                           static_cast<uint32_t>(all_meshlet_vertices.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(all_meshlet_vertices.data());

      meshletVerticesBuffer = std::make_unique<Buffer>(device,
                                                       sizeof(unsigned int),
                                                       static_cast<uint32_t>(all_meshlet_vertices.size()),
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(), meshletVerticesBuffer->getBuffer(), bufferSize, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    // Meshlet Triangles Buffer
    {
      VkDeviceSize const bufferSize = sizeof(unsigned char) * all_meshlet_triangles.size();

      Buffer stagingBuffer{device,
                           sizeof(unsigned char),
                           static_cast<uint32_t>(all_meshlet_triangles.size()),
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      stagingBuffer.map();
      stagingBuffer.writeToBuffer(all_meshlet_triangles.data());

      meshletTrianglesBuffer = std::make_unique<Buffer>(device,
                                                        sizeof(unsigned char),
                                                        static_cast<uint32_t>(all_meshlet_triangles.size()),
                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      device.memory().copyBufferImmediate(stagingBuffer.getBuffer(), meshletTrianglesBuffer->getBuffer(), bufferSize, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    // Log meshlet generation statistics
    if (max_radius > 0.0f && splitMeshletCount > originalMeshletCount)
    {
      std::cout << "[" << GREEN << "Model" << RESET << "] Generated " << meshlets.size() << " meshlets (V=" << max_vertices << ", T=" << max_triangles << ", W=" << cone_weight << ", R=" << max_radius
                << "m). Split " << originalMeshletCount << " -> " << splitMeshletCount << " for better culling." << '\n';
    }
    else
    {
      std::cout << "[" << GREEN << "Model" << RESET << "] Generated " << meshlets.size() << " meshlets (V=" << max_vertices << ", T=" << max_triangles << ", W=" << cone_weight << ")." << '\n';
    }
  }

} // namespace engine
