#include "Geometry.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

namespace LightmapBaker {

  std::vector<Tri> collectTrianglesFromBuilder(const engine::Model::Builder& builder, const glm::mat4& objectTransform, const ExtractOptions& opts)
  {
    std::vector<Tri> out;

    // Iterate over nodes (scene graph). Each node may reference a mesh index.
    const auto& nodes = builder.nodes;
    for (size_t ni = 0; ni < nodes.size(); ++ni)
    {
      if (opts.nodeIndex && static_cast<int>(ni) != *opts.nodeIndex) continue;
      const auto& node = nodes[ni];
      if (node.mesh < 0) continue;
      int meshIndex = node.mesh;
      if (opts.meshIndex && meshIndex != *opts.meshIndex) continue;

      glm::mat4 nodeWorld = objectTransform * node.getLocalTransform();
      glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(nodeWorld)));

      // A mesh may have multiple submeshes; subMeshes in Builder are by material. We need to iterate indices
      for (size_t si = 0; si < builder.subMeshes.size(); ++si)
      {
        const auto& sub = builder.subMeshes[si];
        // We don't have a direct mapping meshIndex->submesh ranges here; so conservatively scan indices and select triples whose material matches
        // Simpler approach for now: iterate indices and take triangles whose materialId (vertex.materialId) corresponds to the submesh.materialId range.
        // Build triangles from full index list but only include those whose indices lie inside this submesh range.

        uint32_t start = sub.indexOffset;
        uint32_t end   = sub.indexOffset + sub.indexCount;
        if (start >= builder.indices.size() || end > builder.indices.size()) continue;

        for (uint32_t i = start; i + 2 < end; i += 3)
        {
          uint32_t ia = builder.indices[i + 0];
          uint32_t ib = builder.indices[i + 1];
          uint32_t ic = builder.indices[i + 2];

          if (ia >= builder.vertices.size() || ib >= builder.vertices.size() || ic >= builder.vertices.size()) continue;

          const auto& va = builder.vertices[ia];
          const auto& vb = builder.vertices[ib];
          const auto& vc = builder.vertices[ic];

          Tri t;
          t.p0 = glm::vec3(nodeWorld * glm::vec4(va.position, 1.0f));
          t.p1 = glm::vec3(nodeWorld * glm::vec4(vb.position, 1.0f));
          t.p2 = glm::vec3(nodeWorld * glm::vec4(vc.position, 1.0f));

          t.n0 = glm::normalize(normalMat * va.normal);
          t.n1 = glm::normalize(normalMat * vb.normal);
          t.n2 = glm::normalize(normalMat * vc.normal);

          t.uv0 = va.uv;
          t.uv1 = vb.uv;
          t.uv2 = vc.uv;

          t.materialId = sub.materialId;

          out.push_back(t);
        }
      }
    }

    return out;
  }

  tools::uvunwrap::Result generateInstanceUVsForNode(const engine::Model::Builder& builder, int nodeIndex, const glm::mat4& objectTransform, int paddingPx, uint32_t resolution)
  {
    tools::uvunwrap::Result r;

    ExtractOptions opts;
    opts.nodeIndex = nodeIndex;
    auto tris      = collectTrianglesFromBuilder(builder, objectTransform, opts);
    if (tris.empty()) return r;

    // Flatten triangle positions into a temporary mesh (duplicate vertices per face)
    std::vector<float>    positions;
    std::vector<uint32_t> indices;
    positions.reserve(tris.size() * 9);
    indices.reserve(tris.size() * 3);

    for (size_t i = 0; i < tris.size(); ++i)
    {
      const auto& t = tris[i];
      positions.push_back(t.p0.x);
      positions.push_back(t.p0.y);
      positions.push_back(t.p0.z);
      positions.push_back(t.p1.x);
      positions.push_back(t.p1.y);
      positions.push_back(t.p1.z);
      positions.push_back(t.p2.x);
      positions.push_back(t.p2.y);
      positions.push_back(t.p2.z);

      indices.push_back(static_cast<uint32_t>(i * 3 + 0));
      indices.push_back(static_cast<uint32_t>(i * 3 + 1));
      indices.push_back(static_cast<uint32_t>(i * 3 + 2));
    }

    tools::uvunwrap::MeshDecl mesh{};
    mesh.vertexPositionData = positions.data();
    mesh.vertexStride       = sizeof(float) * 3;
    mesh.indexData          = indices.data();
    mesh.indexCount         = static_cast<uint32_t>(indices.size());
    mesh.vertexCount        = static_cast<uint32_t>(positions.size() / 3);
    mesh.indexStride        = sizeof(uint32_t);

    r = tools::uvunwrap::generateInstanceUVs(mesh, glm::mat4(1.0f), paddingPx, resolution);

    return r;
  }

  PerVertexUVResult generatePerVertexUVsForNode(const engine::Model::Builder& builder, int nodeIndex, const glm::mat4& objectTransform, int paddingPx, uint32_t resolution)
  {
    PerVertexUVResult out;

    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(builder.nodes.size())) return out;

    const auto& node = builder.nodes[nodeIndex];
    if (node.mesh < 0) return out;

    glm::mat4 nodeWorld = objectTransform * node.getLocalTransform();

    // Map original vertex indices -> local sequential indices and produce a local mesh.
    std::unordered_map<uint32_t, uint32_t> origToLocal;
    std::vector<float>                     localPositions;
    std::vector<uint32_t>                  localIndices;
    localPositions.reserve(builder.indices.size());
    localIndices.reserve(builder.indices.size());

    // Reuse the submesh-based scan used by collectTrianglesFromBuilder to determine triangles
    for (const auto& sub : builder.subMeshes)
    {
      uint32_t start = sub.indexOffset;
      uint32_t end   = sub.indexOffset + sub.indexCount;
      if (start >= builder.indices.size() || end > builder.indices.size()) continue;

      for (uint32_t i = start; i + 2 < end; i += 3)
      {
        uint32_t ia = builder.indices[i + 0];
        uint32_t ib = builder.indices[i + 1];
        uint32_t ic = builder.indices[i + 2];

        auto ensure = [&](uint32_t orig) -> uint32_t {
          auto it = origToLocal.find(orig);
          if (it != origToLocal.end()) return it->second;
          uint32_t  localIdx = static_cast<uint32_t>(localPositions.size() / 3);
          glm::vec3 p        = glm::vec3(nodeWorld * glm::vec4(builder.vertices[orig].position, 1.0f));
          localPositions.push_back(p.x);
          localPositions.push_back(p.y);
          localPositions.push_back(p.z);
          origToLocal[orig] = localIdx;
          return localIdx;
        };

        uint32_t la = ensure(ia);
        uint32_t lb = ensure(ib);
        uint32_t lc = ensure(ic);

        localIndices.push_back(la);
        localIndices.push_back(lb);
        localIndices.push_back(lc);
      }
    }

    if (localPositions.empty() || localIndices.empty()) return out;

    // Build a MeshDecl using local vertex arrays
    tools::uvunwrap::MeshDecl mesh{};
    mesh.vertexPositionData = localPositions.data();
    mesh.vertexStride       = sizeof(float) * 3;
    mesh.indexData          = localIndices.data();
    mesh.indexCount         = static_cast<uint32_t>(localIndices.size());
    mesh.vertexCount        = static_cast<uint32_t>(localPositions.size() / 3);
    mesh.indexStride        = sizeof(uint32_t);

    // Run xatlas packing & parameterization
    tools::uvunwrap::Result res = tools::uvunwrap::generateInstanceUVs(mesh, glm::mat4(1.0f), paddingPx, resolution);

    // Map returned per-local-vertex UVs back to original builder vertex indices
    out.uvPerVertex.resize(builder.vertices.size(), glm::vec2(0.0f));
    out.used.resize(builder.vertices.size(), 0);

    if (res.uv1.size() == mesh.vertexCount)
    {
      for (const auto& kv : origToLocal)
      {
        uint32_t origIdx         = kv.first;
        uint32_t localIdx        = kv.second;
        out.uvPerVertex[origIdx] = res.uv1[localIdx];
        out.used[origIdx]        = 1;
      }
    }

    out.atlasResult = res;
    return out;
  }

  // Collect triangles across the provided scene. The modelLoader callback should load a Model::Builder
  // given a modelPath and return true on success. Each object's transform is applied to all its model nodes.
  std::vector<Tri> collectTrianglesFromScene(const Scene& scene, const std::function<bool(const std::string&, engine::Model::Builder&)>& modelLoader, const ExtractOptions& opts)
  {
    std::vector<Tri> out;

    for (const auto& obj : scene.objects)
    {
      if (!obj.modelPath) continue;
      engine::Model::Builder builder;
      if (!modelLoader(*obj.modelPath, builder)) continue;

      // If there are no nodes, create a temporary node so we still collect triangles
      if (builder.nodes.empty())
      {
        engine::Model::Builder tmp = builder;
        engine::Model::Node    n;
        n.mesh        = 0;
        n.translation = glm::vec3(0.0f);
        tmp.nodes.push_back(n);
        auto tris = collectTrianglesFromBuilder(tmp, obj.transform, opts);
        out.insert(out.end(), tris.begin(), tris.end());
      }
      else
      {
        // Collect triangles from the builder once with the object's transform applied
        auto tris = collectTrianglesFromBuilder(builder, obj.transform, opts);
        out.insert(out.end(), tris.begin(), tris.end());
      }
    }

    return out;
  }

} // namespace LightmapBaker
