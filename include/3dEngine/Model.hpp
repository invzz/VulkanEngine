#pragma once

#ifndef MODEL_PATH
#define MODEL_PATH "assets/models/"
#endif

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
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
      // Texture paths from MTL file (relative to OBJ file location)
      std::string diffuseTexPath;
      std::string normalTexPath;
      std::string roughnessTexPath;
      std::string aoTexPath;
    };

    // Sub-mesh: a portion of the model using one material
    struct SubMesh
    {
      uint32_t indexOffset; // Offset into the index buffer
      uint32_t indexCount;  // Number of indices for this sub-mesh
      int      materialId;  // Index into materials array
    };

    // Animation structures
    struct AnimationSampler
    {
      enum Interpolation
      {
        LINEAR,
        STEP,
        CUBICSPLINE
      };

      std::vector<float>     times;        // Keyframe timestamps
      std::vector<glm::vec3> translations; // For translation channels
      std::vector<glm::quat> rotations;    // For rotation channels
      std::vector<glm::vec3> scales;       // For scale channels
      Interpolation          interpolation = LINEAR;
    };

    struct AnimationChannel
    {
      enum TargetPath
      {
        TRANSLATION,
        ROTATION,
        SCALE
      };

      int        targetNode; // Index into the node array
      TargetPath path;
      int        samplerIndex;
    };

    struct Animation
    {
      std::string                   name;
      float                         duration = 0.0f; // In seconds
      std::vector<AnimationChannel> channels;
      std::vector<AnimationSampler> samplers;
    };

    struct Node
    {
      std::string      name;
      glm::vec3        translation = glm::vec3(0.0f);
      glm::quat        rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      glm::vec3        scale       = glm::vec3(1.0f);
      glm::mat4        matrix      = glm::mat4(1.0f);
      std::vector<int> children;
      int              mesh = -1;

      glm::mat4 getLocalTransform() const
      {
        return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
      }
    };

    struct Builder
    {
      std::vector<Vertex>       vertices{};
      std::vector<uint32_t>     indices{};
      std::vector<MaterialInfo> materials{};  // Materials loaded from MTL file
      std::vector<SubMesh>      subMeshes{};  // Sub-meshes by material
      std::vector<Animation>    animations{}; // Animations from glTF
      std::vector<Node>         nodes{};      // Scene graph nodes

      void loadModelFromFile(const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
      void loadModelFromGLTF(const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
    };

    explicit Model(Device& device, const Builder& builder);
    ~Model();

    // delete copy and move constructors and assignment operators
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
    static std::unique_ptr<Model> createModelFromGLTF(Device& device, const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

    // Draw a specific sub-mesh
    void drawSubMesh(VkCommandBuffer commandBuffer, size_t subMeshIndex) const;

    // Get materials loaded from MTL file
    const std::vector<MaterialInfo>& getMaterials() const { return materials_; }
    std::vector<MaterialInfo>&       getMaterials() { return materials_; }

    // Get sub-meshes
    const std::vector<SubMesh>& getSubMeshes() const { return subMeshes_; }

    // Check if model has multiple materials
    bool hasMultipleMaterials() const { return subMeshes_.size() > 1; }

    // Animation support
    bool                          hasAnimations() const { return !animations_.empty(); }
    const std::vector<Animation>& getAnimations() const { return animations_; }
    const std::vector<Node>&      getNodes() const { return nodes_; }
    std::vector<Node>&            getNodes() { return nodes_; }

  private:
    Device& device;

    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t                vertexCount = 0;

    bool hasIndexBuffer = false;

    std::unique_ptr<Buffer> indexBuffer;
    uint32_t                indexCount = 0;

    std::vector<MaterialInfo> materials_;  // Materials from MTL file
    std::vector<SubMesh>      subMeshes_;  // Sub-meshes by material
    std::vector<Animation>    animations_; // Animations from glTF
    std::vector<Node>         nodes_;      // Scene graph nodes

    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
  };

} // namespace engine
