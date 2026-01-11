#pragma once

#include <ModelLib/Resources/Model.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "Scene.hpp"
#include "Tools/UVUnwrap/UVUnwrap.hpp"

namespace LightmapBaker {

  struct Tri
  {
    glm::vec3 p0, p1, p2;
    glm::vec3 n0, n1, n2;
    glm::vec2 uv0, uv1, uv2;
    int       materialId = -1;
  };

  // Options to filter extraction (mesh index or node index)
  struct ExtractOptions
  {
    // If set, only collect triangles from meshes with this index
    std::optional<int> meshIndex;
    // If set, only collect triangles from nodes with this index
    std::optional<int> nodeIndex;
    // If set, only collect triangles belonging to this primitive index (glTF primitive index)
    std::optional<int> primitiveIndex;
  };

  // Collect triangles for an object (given model builder and an object transform).
  // Applies node local transforms and the provided objectTransform to triangle positions
  // and normals. Returns collected triangles.
  std::vector<Tri> collectTrianglesFromBuilder(const engine::Model::Builder& builder, const glm::mat4& objectTransform, const ExtractOptions& opts = {});

  // Generate lightmap UVs for a node in the model builder using xatlas via UVUnwrap tool.
  // Returns the UVUnwrap::Result which includes per-vertex UVs, atlas sizes and chart metadata.
  tools::uvunwrap::Result generateInstanceUVsForNode(const engine::Model::Builder& builder, int nodeIndex, const glm::mat4& objectTransform, int paddingPx = 4, uint32_t resolution = 0);

  // Per-vertex UV result mapped back to the original builder vertex indices.
  struct PerVertexUVResult
  {
    std::vector<glm::vec2>  uvPerVertex; // size == builder.vertices.size(); values undefined for unused vertices
    std::vector<char>       used;        // flags for which original vertices were used in the generated mesh
    tools::uvunwrap::Result atlasResult; // underlying atlas result from UVUnwrap
  };

  // Generate per-original-vertex UV1 coordinates for the specified node. The returned
  // PerVertexUVResult maps xatlas-generated UVs back onto the Model::Builder vertex indices.
  PerVertexUVResult generatePerVertexUVsForNode(const engine::Model::Builder& builder, int nodeIndex, const glm::mat4& objectTransform, int paddingPx = 4, uint32_t resolution = 0);

  // Collect triangles for an entire scene using a model loader callback. The loader should
  // fill `out` with the model builder when given a model path, and return true on success.
  // The callback signature is: bool loader(const std::string& modelPath, engine::Model::Builder& out)
  std::vector<Tri> collectTrianglesFromScene(const Scene& scene, const std::function<bool(const std::string&, engine::Model::Builder&)>& modelLoader, const ExtractOptions& opts = {});

} // namespace LightmapBaker
