#pragma once
#ifndef MODEL_PATH
#define MODEL_PATH "assets/models/"
#endif
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <limits>
#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/PBRMaterial.hpp"
namespace engine {
    /**
 * @brief Axis-aligned bounding box
 */
    struct AABB {
        glm::vec3 min{std::numeric_limits<float>::max()};
        glm::vec3 max{std::numeric_limits<float>::lowest()};
        void      expand(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
        [[nodiscard]] glm::vec3 center() const {
            return 0.5f * (min + max);
        }
        [[nodiscard]] glm::vec3 extents() const {
            return 0.5f * (max - min);
        }
        [[nodiscard]] bool isValid() const {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }
    };
    /**
 * @brief Transform a local-space AABB to world space
 */
    inline AABB transformAABB(const AABB& local, const glm::mat4& world) {
        glm::vec3 corners[8] = {
            glm::vec3(world * glm::vec4(local.min.x, local.min.y, local.min.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.max.x, local.min.y, local.min.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.min.x, local.max.y, local.min.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.max.x, local.max.y, local.min.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.min.x, local.min.y, local.max.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.max.x, local.min.y, local.max.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.min.x, local.max.y, local.max.z, 1.0f)),
            glm::vec3(world * glm::vec4(local.max.x, local.max.y, local.max.z, 1.0f)),
        };
        AABB result;
        for (const auto& c : corners) {
            result.expand(c);
        }
        return result;
    }
    struct MeshBuffers {
        uint64_t vertexBufferAddress;
        uint64_t indexBufferAddress;
    };
    class Model {
       public:
        struct MeshletBuildConfig {
            size_t maxVertices  = 64;
            size_t maxTriangles = 124;
            float  coneWeight   = 0.0f;
            float  maxRadius    = 0.0f;
        };
        static void                                    setMeshletBuildConfig(const MeshletBuildConfig& cfg);
        [[nodiscard]] static const MeshletBuildConfig& getMeshletBuildConfig();
        struct Vertex {
            glm::vec3                                             position;
            glm::vec3                                             color;
            glm::vec3                                             normal;
            glm::vec2                                             uv;
            int                                                   materialId{-1};
            static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            bool                                                  operator==(const Vertex& other) const {
                return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
            }
        };
        struct MaterialInfo {
            std::string name;
            PBRMaterial pbrMaterial;
            int         materialId;
            std::string diffuseTexPath;
            std::string normalTexPath;
            std::string roughnessTexPath;
            std::string aoTexPath;
            std::string emissiveTexPath;
            std::string specularGlossinessTexPath;
            std::string transmissionTexPath;
            std::string clearcoatTexPath;
            std::string clearcoatRoughnessTexPath;
            std::string clearcoatNormalTexPath;
        };
        struct SubMesh {
            uint32_t indexOffset;
            uint32_t indexCount;
            int      materialId;
            uint32_t meshletOffset = 0;
            uint32_t meshletCount  = 0;
            int      nodeIndex = -1;  // glTF node that instantiates this sub-mesh (-1 if not node-bound)
        };
        struct AnimationSampler {
            enum Interpolation : std::uint8_t {
                LINEAR,
                STEP,
                CUBICSPLINE
            };
            std::vector<float>              times;
            std::vector<glm::vec3>          translations;
            std::vector<glm::quat>          rotations;
            std::vector<glm::vec3>          scales;
            std::vector<std::vector<float>> morphWeights;
            Interpolation                   interpolation = LINEAR;
        };
        struct AnimationChannel {
            enum TargetPath : std::uint8_t {
                TRANSLATION,
                ROTATION,
                SCALE,
                WEIGHTS
            };
            int        targetNode;
            TargetPath path;
            int        samplerIndex;
        };
        struct Animation {
            std::string                   name;
            float                         duration = 0.0f;
            std::vector<AnimationChannel> channels;
            std::vector<AnimationSampler> samplers;
        };
        struct Node {
            std::string             name;
            glm::vec3               translation = glm::vec3(0.0f);
            glm::quat               rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3               scale       = glm::vec3(1.0f);
            glm::mat4               matrix      = glm::mat4(1.0f);
            bool                    hasMatrix   = false;
            std::vector<int>        children;
            int                     mesh = -1;
            std::vector<float>      morphWeights;
            [[nodiscard]] glm::mat4 getLocalTransform() const {
                if (hasMatrix) {
                    return matrix;
                }
                return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
            }
        };
        struct MorphTarget {
            std::vector<glm::vec3> positionDeltas;
            std::vector<glm::vec3> normalDeltas;
            std::string            name;
        };
        struct MorphTargetSet {
            std::vector<MorphTarget> targets;
            std::vector<float>       weights;
            uint32_t                 vertexOffset;
            uint32_t                 vertexCount;
            std::vector<uint32_t>    positionIndices;
        };
        struct Meshlet {
            uint32_t vertexOffset;
            uint32_t triangleOffset;
            uint32_t vertexCount;
            uint32_t triangleCount;
            float    center[3];
            float    radius;
        };
        enum class LightType : uint8_t {
            Point,
            Directional,
            Spot,
        };
        struct LightInfo {
            std::string      name;
            LightType        type{LightType::Point};
            glm::vec3        color{1.0f};
            float            intensity{1.0f};
            float            range{0.0f};
            float            innerCutoffAngle{12.5f};
            float            outerCutoffAngle{17.5f};
            std::vector<int> nodeIndices;
        };
        struct Builder {
            std::vector<Vertex>                       vertices;
            std::vector<uint32_t>                     indices;
            std::vector<MaterialInfo>                 materials;
            std::vector<SubMesh>                      subMeshes;
            std::unordered_map<int, int>              meshPrimaryMaterial;
            std::vector<Animation>                    animations;
            std::vector<Node>                         nodes;
            std::unordered_map<int, std::vector<int>> nodePrimitiveIndices;
            std::unordered_map<uint64_t, uint32_t> primitiveVertexOffsets;
            std::unordered_map<uint64_t, uint32_t> primitiveVertexCounts;
            std::vector<MorphTargetSet>               morphTargetSets;
            std::vector<LightInfo>                    lights;
            std::string                               filePath;
            void                                      loadModelFromFile(const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
            void                                      loadModelFromGLTF(const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
        };
        explicit Model(Device& device, const Builder& builder);
        ~Model();
        Model(const Model&)                                                    = delete;
        Model&                                         operator=(const Model&) = delete;
        static std::unique_ptr<Model>                  createModelFromFile(Device& device, const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
        static std::unique_ptr<Model>                  createModelFromGLTF(Device& device, const std::string& filepath, bool flipX = false, bool flipY = false, bool flipZ = false);
        void                                           bind(VkCommandBuffer commandBuffer) const;
        void                                           draw(VkCommandBuffer commandBuffer) const;
        void                                           drawSubMesh(VkCommandBuffer commandBuffer, size_t subMeshIndex) const;
        [[nodiscard]] int                              getPrimaryMaterialForMesh(int meshIndex) const;
        [[nodiscard]] const std::vector<MaterialInfo>& getMaterials() const {
            return materials_;
        }
        std::vector<MaterialInfo>& getMaterials() {
            return materials_;
        }
        [[nodiscard]] const std::vector<SubMesh>& getSubMeshes() const {
            return subMeshes_;
        }
        [[nodiscard]] bool hasMultipleMaterials() const {
            return subMeshes_.size() > 1;
        }
        [[nodiscard]] bool hasAnimations() const {
            return !animations_.empty();
        }
        [[nodiscard]] const std::vector<Animation>& getAnimations() const {
            return animations_;
        }
        [[nodiscard]] const std::vector<Node>& getNodes() const {
            return nodes_;
        }
        std::vector<Node>& getNodes() {
            return nodes_;
        }
        [[nodiscard]] bool hasMorphTargets() const {
            return !morphTargetSets_.empty();
        }
        [[nodiscard]] const std::vector<MorphTargetSet>& getMorphTargetSets() const {
            return morphTargetSets_;
        }
        std::vector<MorphTargetSet>& getMorphTargetSets() {
            return morphTargetSets_;
        }
        [[nodiscard]] bool hasLights() const {
            return !lights_.empty();
        }
        [[nodiscard]] const std::vector<LightInfo>& getLights() const {
            return lights_;
        }
        [[nodiscard]] VkBuffer getVertexBuffer() const {
            return vertexBuffer->getBuffer();
        }
        [[nodiscard]] VkBuffer getIndexBuffer() const {
            return indexBuffer ? indexBuffer->getBuffer() : VK_NULL_HANDLE;
        }
        [[nodiscard]] uint64_t getVertexBufferAddress() const {
            return vertexBuffer->getDeviceAddress();
        }
        [[nodiscard]] uint64_t getIndexBufferAddress() const {
            return indexBuffer ? indexBuffer->getDeviceAddress() : 0;
        }
        void bindAlternateVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer) const;
        /**
   * @brief Get approximate memory size of this model
   * @return Memory size in bytes (vertex + index buffers)
   */
        [[nodiscard]] size_t             getMemorySize() const;
        [[nodiscard]] const std::string& getFilePath() const {
            return filePath;
        }
        void setMeshId(uint32_t id) {
            meshId = id;
        }
        [[nodiscard]] uint32_t getMeshId() const {
            return meshId;
        }
        [[nodiscard]] const std::vector<Meshlet>& getMeshlets() const {
            return meshlets;
        }
        [[nodiscard]] uint64_t getMeshletBufferAddress() const {
            return meshletBuffer ? meshletBuffer->getDeviceAddress() : 0;
        }
        [[nodiscard]] uint64_t getMeshletVerticesAddress() const {
            return meshletVerticesBuffer ? meshletVerticesBuffer->getDeviceAddress() : 0;
        }
        [[nodiscard]] uint64_t getMeshletTrianglesAddress() const {
            return meshletTrianglesBuffer ? meshletTrianglesBuffer->getDeviceAddress() : 0;
        }
        [[nodiscard]] uint32_t getMeshletCount() const {
            return static_cast<uint32_t>(meshlets.size());
        }
        [[nodiscard]] const AABB& getLocalBounds() const {
            return localBounds_;
        }
        [[nodiscard]] uint32_t getIndexCount() const {
            return indexCount;
        }
        [[nodiscard]] uint32_t getVertexCount() const {
            return vertexCount;
        }
        [[nodiscard]] bool hasIndices() const {
            return hasIndexBuffer;
        }
        [[nodiscard]] const std::vector<glm::vec3>& getCollisionVertices() const {
            return collisionVertices_;
        }
        [[nodiscard]] const std::vector<uint32_t>& getCollisionIndices() const {
            return collisionIndices_;
        }

       private:
        static MeshletBuildConfig    s_meshletConfig_;
        Device&                      device;
        std::string                  filePath;
        uint32_t                     meshId = 0;
        std::unique_ptr<Buffer>      vertexBuffer;
        uint32_t                     vertexCount    = 0;
        bool                         hasIndexBuffer = false;
        std::unique_ptr<Buffer>      indexBuffer;
        uint32_t                     indexCount = 0;
        std::vector<Meshlet>         meshlets;
        std::unique_ptr<Buffer>      meshletBuffer;
        std::unique_ptr<Buffer>      meshletVerticesBuffer;
        std::unique_ptr<Buffer>      meshletTrianglesBuffer;
        std::vector<MaterialInfo>    materials_;
        std::vector<SubMesh>         subMeshes_;
        std::unordered_map<int, int> meshPrimaryMaterial_;
        std::vector<Animation>       animations_;
        std::vector<Node>            nodes_;
        std::vector<MorphTargetSet>  morphTargetSets_;
        std::vector<LightInfo>       lights_;
        AABB                         localBounds_;
        std::vector<glm::vec3>       collisionVertices_;
        std::vector<uint32_t>        collisionIndices_;
        void                         createVertexBuffers(const std::vector<Vertex>& vertices);
        void                         createIndexBuffers(const std::vector<uint32_t>& indices);
        void                         generateMeshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    };
}  // namespace engine
