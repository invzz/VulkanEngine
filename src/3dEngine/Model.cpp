#include "3dEngine/Model.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cassert>
#include <cmath>
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

  // Helper functions for MTL to PBR conversion
  namespace {

    PBRMaterial mtlToPBR(const glm::vec3& Kd, const glm::vec3& Ks, float Ns, std::string& matName)
    {
      // Convert MTL (Phong) material to PBR material
      // Note: This conversion is heuristic-based. Future improvements:
      // - Transparency: Use MTL 'd' parameter for alpha/transmission (HeadlightGlass, Glass)
      // - Clearcoat: Add second specular layer for car paint (roughness ~0.05)
      // - Anisotropy: For brushed metals or fabric materials

      PBRMaterial pbr;

      // Roughness from Ns
      Ns              = glm::clamp(Ns, 1.0f, 1000.0f);
      float roughness = sqrtf(2.0f / (Ns + 2.0f));
      roughness       = powf(roughness, 0.5f);
      roughness       = glm::clamp(roughness, 0.0f, 1.0f);

      float specularIntensity = glm::clamp((Ks.r + Ks.g + Ks.b) / 3.0f, 0.0f, 1.0f);
      float kdLuminance       = glm::dot(Kd, glm::vec3(0.299f, 0.587f, 0.114f));

      // --- Force metals by name
      std::string nameLower = matName;
      std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
      bool forceMetal = (nameLower.find("chrome") != std::string::npos || nameLower.find("mirror") != std::string::npos ||
                         nameLower.find("aluminum") != std::string::npos || nameLower.find("metal") != std::string::npos);

      if (forceMetal || (glm::length(Kd) < 0.05f && specularIntensity > 0.6f))
      {
        pbr.metallic = 1.0f;
        roughness    = glm::min(roughness, 0.02f); // mirror-like

        // Use specular color for metallic tint (gold, copper, chrome with tint)
        // This preserves colored reflections for metals
        pbr.albedo = glm::clamp(Ks, glm::vec3(0.04f), glm::vec3(1.0f));
      }
      else
      {
        // Non-obvious metals
        float maxKs      = glm::max(glm::max(Ks.r, Ks.g), Ks.b);
        float minKs      = glm::min(glm::min(Ks.r, Ks.g), Ks.b);
        float tintAmount = glm::clamp((maxKs - minKs) / glm::max(maxKs, 1e-6f), 0.0f, 1.0f);

        float metallic = glm::smoothstep(0.05f, 0.25f, tintAmount);
        metallic *= glm::smoothstep(0.05f, 0.0f, kdLuminance);
        pbr.metallic = glm::clamp(metallic, 0.0f, 1.0f);

        // Blend albedo based on metallic value for partial metals
        pbr.albedo = glm::mix(Kd, glm::clamp(Ks, glm::vec3(0.04f), glm::vec3(1.0f)), pbr.metallic);
      }

      pbr.roughness = roughness;
      pbr.ao        = 1.0f;

      // --- Detect clearcoat materials (car paint, lacquered surfaces)
      bool isCarPaint =
              (nameLower.find("bmw") != std::string::npos || nameLower.find("carshell") != std::string::npos || nameLower.find("paint") != std::string::npos);

      if (isCarPaint && pbr.metallic < 0.5f) // Dielectric paint with clearcoat
      {
        pbr.clearcoat          = 1.0f;  // Full clearcoat layer
        pbr.clearcoatRoughness = 0.05f; // Smooth glossy finish
      }

      // --- Detect anisotropic materials (brushed metals)
      bool isBrushedMetal = (nameLower.find("brushed") != std::string::npos || nameLower.find("aluminum") != std::string::npos ||
                             nameLower.find("steel") != std::string::npos);

      if (isBrushedMetal)
      {
        pbr.anisotropic         = 0.8f; // Strong anisotropic effect
        pbr.anisotropicRotation = 0.0f; // Aligned with tangent
      }

      return pbr;
    }
  } // namespace

  Model::Model(Device& device, const Builder& builder) : device{device}, materials_{builder.materials}, subMeshes_{builder.subMeshes}
  {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
  }

  std::unique_ptr<Model> Model::createModelFromFile(Device& device, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading model from file: " << filepath << std::endl;
    Builder builder;
    builder.loadModelFromFile(std::string(MODEL_PATH) + filepath, flipX, flipY, flipZ);
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
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> tinyMaterials;
    std::string                      warn;
    std::string                      err;

    // Get the directory path for MTL file

    if (std::string mtlBaseDir = filepath.substr(0, filepath.find_last_of("/\\") + 1);
        !tinyobj::LoadObj(&attrib, &shapes, &tinyMaterials, &warn, &err, filepath.c_str(), mtlBaseDir.c_str()))
    {
      throw std::runtime_error(warn + err);
    }

#if defined(DEBUG)
    if (!warn.empty())
    {
      std::cout << YELLOW << "[Model] Warning: " << RESET << warn << std::endl;
    }
#endif

    // Convert TinyObj materials to our PBR materials
    materials.clear();
    for (const auto& mat : tinyMaterials)
    {
      MaterialInfo matInfo;
      matInfo.name = mat.name;

      // Convert TinyObj material to PBR using helper function
      auto  Kd = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
      auto  Ks = glm::vec3(mat.specular[0], mat.specular[1], mat.specular[2]);
      float Ns = mat.shininess;

      matInfo.pbrMaterial = mtlToPBR(Kd, Ks, Ns, matInfo.name);
      matInfo.materialId  = static_cast<int>(materials.size());
      materials.push_back(matInfo);

      std::cout << "[" << GREEN << " Material " << RESET << "] " << BLUE << mat.name << RESET << " -> PBR(albedo=" << matInfo.pbrMaterial.albedo.r << ","
                << matInfo.pbrMaterial.albedo.g << "," << matInfo.pbrMaterial.albedo.b << ", metallic=" << matInfo.pbrMaterial.metallic
                << ", roughness=" << matInfo.pbrMaterial.roughness << ")" << std::endl;
    }

    // Group indices by material to create sub-meshes
    std::unordered_map<int, std::vector<uint32_t>> indicesByMaterial;

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    vertices.clear();
    indices.clear();

    float xMultiplier = flipX ? -1.0f : 1.0f;
    float yMultiplier = flipY ? -1.0f : 1.0f;
    float zMultiplier = flipZ ? -1.0f : 1.0f;

    for (const auto& shape : shapes)
    {
      for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++)
      {
        // Get material ID for this face
        int materialId = shape.mesh.material_ids[f];

        for (size_t v = 0; v < 3; v++)
        {
          const auto& index = shape.mesh.indices[3 * f + v];
          Vertex      vertex{};
          vertex.materialId = materialId;

          if (index.vertex_index >= 0)
          {
            vertex.position = {
                    xMultiplier * attrib.vertices[3 * index.vertex_index + 0],
                    yMultiplier * attrib.vertices[3 * index.vertex_index + 1],
                    zMultiplier * attrib.vertices[3 * index.vertex_index + 2],
            };

            if (!attrib.colors.empty())
            {
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
          }

          if (index.normal_index >= 0)
          {
            vertex.normal = {
                    xMultiplier * attrib.normals[3 * index.normal_index + 0],
                    yMultiplier * attrib.normals[3 * index.normal_index + 1],
                    zMultiplier * attrib.normals[3 * index.normal_index + 2],
            };
          }

          if (index.texcoord_index >= 0)
          {
            vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1],
            };
          }

          if (uniqueVertices.count(vertex) == 0)
          {
            uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex);
          }

          uint32_t vertexIndex = uniqueVertices[vertex];
          indices.push_back(vertexIndex);

          // Group indices by material
          indicesByMaterial[materialId].push_back(vertexIndex);
        }
      }
    }

    // Create sub-meshes from grouped indices
    subMeshes.clear();
    uint32_t currentOffset = 0;

    for (auto& [matId, matIndices] : indicesByMaterial)
    {
      if (!matIndices.empty())
      {
        SubMesh subMesh;
        subMesh.materialId  = matId;
        subMesh.indexOffset = currentOffset;
        subMesh.indexCount  = static_cast<uint32_t>(matIndices.size());
        subMeshes.push_back(subMesh);

        currentOffset += subMesh.indexCount;
      }
    }

    // Rebuild indices array grouped by material
    std::vector<uint32_t> groupedIndices;
    groupedIndices.reserve(indices.size());

    for (const auto& subMesh : subMeshes)
    {
      const auto& matIndices = indicesByMaterial[subMesh.materialId];
      groupedIndices.insert(groupedIndices.end(), matIndices.begin(), matIndices.end());
    }

    indices = std::move(groupedIndices);

    std::cout << GREEN << "[Model] Loaded " << materials.size() << " materials, " << subMeshes.size() << " sub-meshes" << RESET << std::endl;
  }

} // namespace engine
