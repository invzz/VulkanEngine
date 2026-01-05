#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_GLTFIMPORTER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_GLTFIMPORTER_HPP

#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "ModelImporter.hpp"

namespace engine {

  /**
   * @brief Importer for glTF 2.0 files (.gltf and .glb binary format)
   */
  class GLTFImporter : public ModelImporter
  {
  public:
    bool load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ) override;

    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override { return {"gltf", "glb"}; }

    [[nodiscard]] std::string getName() const override { return "glTF Importer"; }

  private:
    // Refactored helper methods to keep `load()` small and testable
    void loadMaterials(Model::Builder& builder, const tinygltf::Model& model, const std::string& baseDir, const std::string& cacheDir);

    void loadMeshes(Model::Builder&                            builder,
                    const tinygltf::Model&                     model,
                    bool                                       flipX,
                    bool                                       flipY,
                    bool                                       flipZ,
                    std::unordered_map<std::string, uint32_t>& primitiveVertexOffsets,
                    std::unordered_map<std::string, uint32_t>& primitiveVertexCounts,
                    std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex,
                    bool                                       hasAnimations);

    void loadMorphTargets(Model::Builder&                                  builder,
                          const tinygltf::Model&                           model,
                          const std::unordered_map<std::string, uint32_t>& primitiveVertexOffsets,
                          const std::unordered_map<std::string, uint32_t>& primitiveVertexCounts,
                          const std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex);

    void loadAnimations(Model::Builder& builder, const tinygltf::Model& model);

    [[nodiscard]] glm::mat4 computeNodeTransform(const tinygltf::Node& node) const;

    void processMesh(Model::Builder&                                 builder,
                     const tinygltf::Model&                          model,
                     int                                             meshIndex,
                     const glm::mat4&                                globalTransform,
                     std::unordered_map<Model::Vertex, uint32_t>&    uniqueVertices,
                     std::unordered_map<int, std::vector<uint32_t>>& indicesByMaterial,
                     std::unordered_map<std::string, uint32_t>&      primitiveVertexOffsets,
                     std::unordered_map<std::string, uint32_t>&      primitiveVertexCounts,
                     std::unordered_map<uint32_t, uint32_t>&         vertexToPositionIndex,
                     bool                                            hasAnimations,
                     float                                           xMultiplier,
                     float                                           yMultiplier,
                     float                                           zMultiplier);
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_GLTFIMPORTER_HPP
