#include "3dEngine/importers/OBJImporter.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "3dEngine/ansi_colors.hpp"
#include "3dEngine/utils.hpp"

// Hash function for Model::Vertex
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

  // Helper function for MTL to PBR conversion
  static PBRMaterial mtlToPBR(const glm::vec3& Kd, const glm::vec3& Ks, float Ns, std::string& matName)
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
    bool isBrushedMetal =
            (nameLower.find("brushed") != std::string::npos || nameLower.find("aluminum") != std::string::npos || nameLower.find("steel") != std::string::npos);

    if (isBrushedMetal)
    {
      pbr.anisotropic         = 0.8f; // Strong anisotropic effect
      pbr.anisotropicRotation = 0.0f; // Aligned with tangent
    }

    return pbr;
  }

  bool OBJImporter::load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> tinyMaterials;
    std::string                      warn;
    std::string                      err;

    // Get the directory path for MTL file
    std::string mtlBaseDir = filepath.substr(0, filepath.find_last_of("/\\") + 1);

    if (!tinyobj::LoadObj(&attrib, &shapes, &tinyMaterials, &warn, &err, filepath.c_str(), mtlBaseDir.c_str()))
    {
      std::cerr << RED << "[OBJImporter] Error: " << RESET << warn + err << std::endl;
      return false;
    }

#if defined(DEBUG)
    if (!warn.empty())
    {
      std::cout << YELLOW << "[OBJImporter] Warning: " << RESET << warn << std::endl;
    }
#endif

    // Convert TinyObj materials to our PBR materials
    builder.materials.clear();
    for (const auto& mat : tinyMaterials)
    {
      Model::MaterialInfo matInfo;
      matInfo.name = mat.name;

      // Convert TinyObj material to PBR using helper function
      auto  Kd = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
      auto  Ks = glm::vec3(mat.specular[0], mat.specular[1], mat.specular[2]);
      float Ns = mat.shininess;

      matInfo.pbrMaterial = mtlToPBR(Kd, Ks, Ns, matInfo.name);
      matInfo.materialId  = static_cast<int>(builder.materials.size());

      // Extract texture paths from MTL file
      matInfo.diffuseTexPath   = mat.diffuse_texname;
      matInfo.normalTexPath    = mat.bump_texname.empty() ? mat.normal_texname : mat.bump_texname;
      matInfo.roughnessTexPath = mat.roughness_texname.empty() ? mat.specular_texname : mat.roughness_texname;
      matInfo.aoTexPath        = mat.ambient_texname;

      builder.materials.push_back(matInfo);

      std::cout << "[" << GREEN << " Material " << RESET << "] " << BLUE << mat.name << RESET << " -> PBR(albedo=" << matInfo.pbrMaterial.albedo.r << ","
                << matInfo.pbrMaterial.albedo.g << "," << matInfo.pbrMaterial.albedo.b << ", metallic=" << matInfo.pbrMaterial.metallic
                << ", roughness=" << matInfo.pbrMaterial.roughness << ")" << std::endl;
    }

    // Group indices by material to create sub-meshes
    std::unordered_map<int, std::vector<uint32_t>> indicesByMaterial;
    std::unordered_map<Model::Vertex, uint32_t>    uniqueVertices{};

    builder.vertices.clear();
    builder.indices.clear();

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
          const auto&   index = shape.mesh.indices[3 * f + v];
          Model::Vertex vertex{};
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
            uniqueVertices[vertex] = static_cast<uint32_t>(builder.vertices.size());
            builder.vertices.push_back(vertex);
          }

          uint32_t vertexIndex = uniqueVertices[vertex];
          builder.indices.push_back(vertexIndex);

          // Group indices by material
          indicesByMaterial[materialId].push_back(vertexIndex);
        }
      }
    }

    // Create sub-meshes from grouped indices
    builder.subMeshes.clear();
    uint32_t currentOffset = 0;

    for (auto& [matId, matIndices] : indicesByMaterial)
    {
      if (!matIndices.empty())
      {
        Model::SubMesh subMesh;
        subMesh.materialId  = matId;
        subMesh.indexOffset = currentOffset;
        subMesh.indexCount  = static_cast<uint32_t>(matIndices.size());
        builder.subMeshes.push_back(subMesh);

        currentOffset += subMesh.indexCount;
      }
    }

    // Rebuild indices array grouped by material
    std::vector<uint32_t> groupedIndices;
    groupedIndices.reserve(builder.indices.size());

    for (const auto& subMesh : builder.subMeshes)
    {
      const auto& matIndices = indicesByMaterial[subMesh.materialId];
      groupedIndices.insert(groupedIndices.end(), matIndices.begin(), matIndices.end());
    }

    builder.indices = std::move(groupedIndices);

    std::cout << GREEN << "[OBJImporter] Loaded " << builder.materials.size() << " materials, " << builder.subMeshes.size() << " sub-meshes" << RESET
              << std::endl;

    return true;
  }

} // namespace engine
