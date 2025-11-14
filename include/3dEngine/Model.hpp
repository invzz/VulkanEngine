#pragma once

#ifndef MODEL_PATH
#define MODEL_PATH "assets/models/"
#endif

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Buffer.hpp"
#include "Device.hpp"
#include "PBRMaterial.hpp"

namespace engine {

  class Model
  {
  public:
    struct Vertex
    {
      glm::vec3                                             position;
      glm::vec3                                             color;
      glm::vec3                                             normal;
      glm::vec2                                             uv;
      int                                                   materialId{-1}; // Material index for this vertex
      static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
      bool operator==(const Vertex& other) const { return position == other.position && color == other.color && normal == other.normal && uv == other.uv; }
    };

    struct MaterialInfo
    {
      std::string name;
      PBRMaterial pbrMaterial;
      int         materialId; // Index in the materials array
    };

    // Sub-mesh: a portion of the model using one material
    struct SubMesh
    {
      uint32_t indexOffset; // Offset into the index buffer
      uint32_t indexCount;  // Number of indices for this sub-mesh
      int      materialId;  // Index into materials array
    };

    struct Builder
    {
      std::vector<Vertex>       vertices{};
      std::vector<uint32_t>     indices{};
      std::vector<MaterialInfo> materials{}; // Materials loaded from MTL file
      std::vector<SubMesh>      subMeshes{}; // Sub-meshes by material

      void loadModelFromFile(const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
    };

    explicit Model(Device& device, const Builder& builder);
    ~Model();

    // delete copy and move constructors and assignment operators
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

    // Draw a specific sub-mesh
    void drawSubMesh(VkCommandBuffer commandBuffer, size_t subMeshIndex) const;

    // Get materials loaded from MTL file
    const std::vector<MaterialInfo>& getMaterials() const { return materials_; }

    // Get sub-meshes
    const std::vector<SubMesh>& getSubMeshes() const { return subMeshes_; }

    // Check if model has multiple materials
    bool hasMultipleMaterials() const { return subMeshes_.size() > 1; }

  private:
    Device& device;

    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t                vertexCount = 0;

    bool hasIndexBuffer = false;

    std::unique_ptr<Buffer> indexBuffer;
    uint32_t                indexCount = 0;

    std::vector<MaterialInfo> materials_; // Materials from MTL file
    std::vector<SubMesh>      subMeshes_; // Sub-meshes by material

    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
  };

} // namespace engine
