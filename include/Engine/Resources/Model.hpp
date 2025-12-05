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

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/PBRMaterial.hpp"

namespace engine {

  struct MeshBuffers
  {
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
  };

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
      std::string emissiveTexPath;
    };

    // Sub-mesh: a portion of the model using one material
    struct SubMesh
    {
      uint32_t indexOffset; // Offset into the index buffer
      uint32_t indexCount;  // Number of indices for this sub-mesh
      int      materialId;  // Index into materials array
      uint32_t meshletOffset = 0;
      uint32_t meshletCount  = 0;
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

      std::vector<float>              times;        // Keyframe timestamps
      std::vector<glm::vec3>          translations; // For translation channels
      std::vector<glm::quat>          rotations;    // For rotation channels
      std::vector<glm::vec3>          scales;       // For scale channels
      std::vector<std::vector<float>> morphWeights; // For morph target weight channels
      Interpolation                   interpolation = LINEAR;
    };

    struct AnimationChannel
    {
      enum TargetPath
      {
        TRANSLATION,
        ROTATION,
        SCALE,
        WEIGHTS // Morph target weights
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
      std::string        name;
      glm::vec3          translation = glm::vec3(0.0f);
      glm::quat          rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      glm::vec3          scale       = glm::vec3(1.0f);
      glm::mat4          matrix      = glm::mat4(1.0f);
      std::vector<int>   children;
      int                mesh = -1;
      std::vector<float> morphWeights; // Current morph target weights

      glm::mat4 getLocalTransform() const
      {
        return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
      }
    };

    // Morph target (blend shape) data
    struct MorphTarget
    {
      std::vector<glm::vec3> positionDeltas; // Position offsets from base mesh
      std::vector<glm::vec3> normalDeltas;   // Normal offsets from base mesh
      std::string            name;
    };

    struct MorphTargetSet
    {
      std::vector<MorphTarget> targets;         // All morph targets for this mesh
      std::vector<float>       weights;         // Current blend weights [0-1]
      uint32_t                 vertexOffset;    // Offset into vertex buffer
      uint32_t                 vertexCount;     // Number of vertices affected
      std::vector<uint32_t>    positionIndices; // Mapping from vertex to original glTF position index
    };

    struct Meshlet
    {
      uint32_t vertexOffset;
      uint32_t triangleOffset;
      uint32_t vertexCount;
      uint32_t triangleCount;

      float center[3];
      float radius;

      float cone_axis[3];
      float cone_cutoff;
    };

    struct Builder
    {
      std::vector<Vertex>         vertices{};
      std::vector<uint32_t>       indices{};
      std::vector<MaterialInfo>   materials{};       // Materials loaded from MTL file
      std::vector<SubMesh>        subMeshes{};       // Sub-meshes by material
      std::vector<Animation>      animations{};      // Animations from glTF
      std::vector<Node>           nodes{};           // Scene graph nodes
      std::vector<MorphTargetSet> morphTargetSets{}; // Morph targets per mesh
      std::string                 filePath{};

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

    // Morph target support
    bool                               hasMorphTargets() const { return !morphTargetSets_.empty(); }
    const std::vector<MorphTargetSet>& getMorphTargetSets() const { return morphTargetSets_; }
    std::vector<MorphTargetSet>&       getMorphTargetSets() { return morphTargetSets_; }

    // Buffer access for compute operations
    VkBuffer getVertexBuffer() const { return vertexBuffer->getBuffer(); }
    VkBuffer getIndexBuffer() const { return indexBuffer ? indexBuffer->getBuffer() : VK_NULL_HANDLE; }

    uint64_t getVertexBufferAddress() const { return vertexBuffer->getDeviceAddress(); }
    uint64_t getIndexBufferAddress() const { return indexBuffer ? indexBuffer->getDeviceAddress() : 0; }

    void bindAlternateVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer) const;

    /**
     * @brief Get approximate memory size of this model
     * @return Memory size in bytes (vertex + index buffers)
     */
    size_t getMemorySize() const;

    const std::string& getFilePath() const { return filePath; }

    void     setMeshId(uint32_t id) { meshId = id; }
    uint32_t getMeshId() const { return meshId; }

    // Meshlet support
    const std::vector<Meshlet>& getMeshlets() const { return meshlets; }
    uint64_t                    getMeshletBufferAddress() const { return meshletBuffer ? meshletBuffer->getDeviceAddress() : 0; }
    uint64_t                    getMeshletVerticesAddress() const { return meshletVerticesBuffer ? meshletVerticesBuffer->getDeviceAddress() : 0; }
    uint64_t                    getMeshletTrianglesAddress() const { return meshletTrianglesBuffer ? meshletTrianglesBuffer->getDeviceAddress() : 0; }
    uint32_t                    getMeshletCount() const { return static_cast<uint32_t>(meshlets.size()); }

  private:
    Device&     device;
    std::string filePath;
    uint32_t    meshId = 0;

    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t                vertexCount = 0;

    bool hasIndexBuffer = false;

    std::unique_ptr<Buffer> indexBuffer;
    uint32_t                indexCount = 0;

    // Meshlet buffers
    std::vector<Meshlet>    meshlets;
    std::unique_ptr<Buffer> meshletBuffer;
    std::unique_ptr<Buffer> meshletVerticesBuffer;
    std::unique_ptr<Buffer> meshletTrianglesBuffer;

    std::vector<MaterialInfo>   materials_;       // Materials from MTL file
    std::vector<SubMesh>        subMeshes_;       // Sub-meshes by material
    std::vector<Animation>      animations_;      // Animations from glTF
    std::vector<Node>           nodes_;           // Scene graph nodes
    std::vector<MorphTargetSet> morphTargetSets_; // Morph targets

    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
    void generateMeshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
  };

} // namespace engine
