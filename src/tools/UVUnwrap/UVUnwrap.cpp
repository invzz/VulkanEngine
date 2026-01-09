#include "Tools/UVUnwrap/UVUnwrap.hpp"

#include <xatlas.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tools::uvunwrap {

  Result generateInstanceUVs(const MeshDecl& mesh, const glm::mat4& instanceTransform, int paddingPx, uint32_t resolution)
  {
    Result r;
    if (!mesh.vertexPositionData || mesh.vertexCount == 0 || !mesh.indexData || mesh.indexCount == 0)
    {
      return r; // empty result
    }

    xatlas::Atlas* atlas = xatlas::Create();

    xatlas::MeshDecl decl;
    decl.vertexPositionData   = mesh.vertexPositionData;
    decl.vertexPositionStride = mesh.vertexStride;
    decl.indexData            = mesh.indexData;
    decl.indexCount           = mesh.indexCount;
    decl.vertexCount          = mesh.vertexCount;
    if (mesh.indexStride == 2)
      decl.indexFormat = xatlas::IndexFormat::UInt16;
    else
      decl.indexFormat = xatlas::IndexFormat::UInt32;

    xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl, 0);
    if (err != xatlas::AddMeshError::Success)
    {
      xatlas::Destroy(atlas);
      throw std::runtime_error("xatlas: failed to add mesh");
    }
    xatlas::AddMeshJoin(atlas);

    xatlas::ChartOptions chartOptions;
    xatlas::PackOptions  packOptions;
    packOptions.padding = static_cast<uint32_t>(paddingPx);
    if (resolution > 0)
    {
      // Caller requested explicit atlas resolution
      packOptions.resolution = resolution;
    }
    else
    {
      // Compute a per-instance texel density from the instance transform scale
      // Use average scale of the transform's basis vectors
      glm::vec3   sx                = glm::vec3(instanceTransform[0]);
      glm::vec3   sy                = glm::vec3(instanceTransform[1]);
      glm::vec3   sz                = glm::vec3(instanceTransform[2]);
      float       scaleAvg          = (glm::length(sx) + glm::length(sy) + glm::length(sz)) / 3.0f;
      const float baseTexelsPerUnit = 64.0f; // sensible default; can be tuned
      packOptions.texelsPerUnit     = baseTexelsPerUnit * scaleAvg;
    }
    packOptions.createImage = false;

    xatlas::Generate(atlas, chartOptions, packOptions);

    // Multi-mesh variant helper: not used here (single mesh path below)
    // but we implement a separate function below for multi-mesh packing.

    // We're assuming a single mesh was added and therefore atlas->meshes[0] corresponds to it.
    if (atlas->meshCount == 0)
    {
      xatlas::Destroy(atlas);
      return r;
    }

    const xatlas::Mesh& outMesh = atlas->meshes[0];
    uint32_t            atlasW  = atlas->width;
    uint32_t            atlasH  = atlas->height;
    r.atlasWidth                = atlasW;
    r.atlasHeight               = atlasH;

    r.uv1.resize(mesh.vertexCount, glm::vec2(0.0f, 0.0f));
    std::vector<char> filled(mesh.vertexCount, 0);

    // Assign UVs for original vertices using xref mapping. xatlas vertex.uv are in texel space.
    for (uint32_t vi = 0; vi < outMesh.vertexCount; ++vi)
    {
      const xatlas::Vertex& v    = outMesh.vertexArray[vi];
      uint32_t              xref = v.xref;
      if (xref >= mesh.vertexCount) continue;
      if (!filled[xref])
      {
        float nx     = v.uv[0] / static_cast<float>(atlasW);
        float ny     = v.uv[1] / static_cast<float>(atlasH);
        r.uv1[xref]  = glm::vec2(nx, ny);
        filled[xref] = 1;
      }
    }

    // Compute bounding rect for UVs (min/max of assigned vertices)
    glm::vec2 minUv{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    glm::vec2 maxUv{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (uint32_t i = 0; i < mesh.vertexCount; ++i)
    {
      if (!filled[i]) continue; // ignore unassigned
      minUv = glm::min(minUv, r.uv1[i]);
      maxUv = glm::max(maxUv, r.uv1[i]);
    }
    if (minUv.x <= maxUv.x && minUv.y <= maxUv.y)
    {
      r.uvOffset = minUv;
      r.uvScale  = maxUv - minUv;
    }

    // Build chart rect metadata (normalized)
    std::vector<ChartRect> charts;
    if (outMesh.chartCount > 0)
    {
      charts.reserve(outMesh.chartCount);
      for (uint32_t ci = 0; ci < outMesh.chartCount; ++ci)
      {
        glm::vec2 cmin{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
        glm::vec2 cmax{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        // iterate vertices and collect those with chartIndex == ci
        for (uint32_t vi = 0; vi < outMesh.vertexCount; ++vi)
        {
          const xatlas::Vertex& v = outMesh.vertexArray[vi];
          if (v.chartIndex != static_cast<int>(ci)) continue;
          glm::vec2 uvNorm{v.uv[0] / static_cast<float>(atlasW), v.uv[1] / static_cast<float>(atlasH)};
          cmin = glm::min(cmin, uvNorm);
          cmax = glm::max(cmax, uvNorm);
        }
        if (cmin.x <= cmax.x && cmin.y <= cmax.y)
        {
          ChartRect cr;
          cr.atlasIndex = outMesh.vertexArray[0].atlasIndex; // atlasIndex for this mesh (usually 0)
          cr.rect       = glm::vec4(cmin.x, cmin.y, cmax.x - cmin.x, cmax.y - cmin.y);
          charts.push_back(cr);
        }
      }
      r.charts = charts;
    }

    xatlas::Destroy(atlas);
    return r;
  }

  MultiResult generateAtlasForMeshes(const std::vector<MeshDecl>& meshes, int paddingPx, uint32_t resolution)
  {
    MultiResult result;
    if (meshes.empty()) return result;

    xatlas::Atlas* atlas = xatlas::Create();

    // Add all meshes
    for (const auto& mesh : meshes)
    {
      if (!mesh.vertexPositionData || mesh.vertexCount == 0 || !mesh.indexData || mesh.indexCount == 0) continue;
      xatlas::MeshDecl decl{};
      decl.vertexPositionData   = mesh.vertexPositionData;
      decl.vertexPositionStride = mesh.vertexStride;
      decl.indexData            = mesh.indexData;
      decl.indexCount           = mesh.indexCount;
      decl.vertexCount          = mesh.vertexCount;
      decl.indexFormat          = (mesh.indexStride == 2) ? xatlas::IndexFormat::UInt16 : xatlas::IndexFormat::UInt32;

      xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl, 0);
      if (err != xatlas::AddMeshError::Success)
      {
        xatlas::Destroy(atlas);
        throw std::runtime_error("xatlas: failed to add mesh in multi-pack");
      }
    }

    xatlas::AddMeshJoin(atlas);

    xatlas::ChartOptions chartOptions;
    xatlas::PackOptions  packOptions;
    packOptions.padding = static_cast<uint32_t>(paddingPx);
    if (resolution > 0)
    {
      packOptions.resolution = resolution;
    }
    // For deterministic unit tests prefer bruteForce packing
    packOptions.bruteForce  = true;
    packOptions.createImage = false;

    xatlas::Generate(atlas, chartOptions, packOptions);

    result.atlasWidth  = atlas->width;
    result.atlasHeight = atlas->height;

    // For each mesh output, gather per-vertex UVs and charts
    result.uvPerMesh.resize(atlas->meshCount);
    result.chartsPerMesh.resize(atlas->meshCount);

    for (uint32_t m = 0; m < atlas->meshCount; ++m)
    {
      const xatlas::Mesh& outMesh = atlas->meshes[m];
      result.uvPerMesh[m].resize(outMesh.vertexCount);

      for (uint32_t vi = 0; vi < outMesh.vertexCount; ++vi)
      {
        const xatlas::Vertex& v = outMesh.vertexArray[vi];
        result.uvPerMesh[m][vi] = glm::vec2(v.uv[0] / static_cast<float>(atlas->width), v.uv[1] / static_cast<float>(atlas->height));
      }

      // collect charts for this mesh
      if (outMesh.chartCount > 0)
      {
        for (uint32_t ci = 0; ci < outMesh.chartCount; ++ci)
        {
          glm::vec2 cmin{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
          glm::vec2 cmax{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
          for (uint32_t vi = 0; vi < outMesh.vertexCount; ++vi)
          {
            const xatlas::Vertex& v = outMesh.vertexArray[vi];
            if (v.chartIndex != static_cast<int>(ci)) continue;
            glm::vec2 uvNorm{v.uv[0] / static_cast<float>(atlas->width), v.uv[1] / static_cast<float>(atlas->height)};
            cmin = glm::min(cmin, uvNorm);
            cmax = glm::max(cmax, uvNorm);
          }
          if (cmin.x <= cmax.x && cmin.y <= cmax.y)
          {
            ChartRect cr;
            cr.atlasIndex = outMesh.vertexArray[0].atlasIndex;
            cr.rect       = glm::vec4(cmin.x, cmin.y, cmax.x - cmin.x, cmax.y - cmin.y);
            result.chartsPerMesh[m].push_back(cr);
          }
        }
      }
    }

    xatlas::Destroy(atlas);
    return result;
  }

  std::vector<InstanceMapping> generateInstanceMappings(const std::vector<std::pair<MeshDecl, glm::mat4>>& meshesWithTransform, int paddingPx, uint32_t resolution)
  {
    std::vector<InstanceMapping> mappings;
    if (meshesWithTransform.empty()) return mappings;

    // To account for per-instance scale we transform vertex positions into
    // instance space so xatlas treats scaled geometry differently and produces
    // proportionally different chart sizes.
    std::vector<MeshDecl> tmeshes;
    tmeshes.reserve(meshesWithTransform.size());
    // Keep position buffers alive while xatlas runs
    std::vector<std::vector<float>> positionBuffers;
    positionBuffers.reserve(meshesWithTransform.size());

    for (const auto& p : meshesWithTransform)
    {
      const MeshDecl&  src = p.first;
      const glm::mat4& xf  = p.second;
      MeshDecl         dst = src;

      // Copy and transform positions into a temporary buffer
      positionBuffers.emplace_back();
      auto& buf = positionBuffers.back();
      buf.resize(static_cast<size_t>(src.vertexCount) * 3);
      const float* srcPos = static_cast<const float*>(src.vertexPositionData);
      for (uint32_t i = 0; i < src.vertexCount; ++i)
      {
        glm::vec4 pos  = glm::vec4(srcPos[i * 3 + 0], srcPos[i * 3 + 1], srcPos[i * 3 + 2], 1.0f);
        glm::vec4 tp   = xf * pos;
        buf[i * 3 + 0] = tp.x;
        buf[i * 3 + 1] = tp.y;
        buf[i * 3 + 2] = tp.z;
      }
      dst.vertexPositionData = buf.data();
      dst.vertexStride       = sizeof(float) * 3;
      tmeshes.push_back(dst);
    }

    MultiResult multi = generateAtlasForMeshes(tmeshes, paddingPx, resolution);
    mappings.resize(multi.uvPerMesh.size());

    for (size_t i = 0; i < multi.uvPerMesh.size(); ++i)
    {
      const auto& uvs = multi.uvPerMesh[i];
      glm::vec2   minUv{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
      glm::vec2   maxUv{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
      for (const auto& uv : uvs)
      {
        minUv = glm::min(minUv, uv);
        maxUv = glm::max(maxUv, uv);
      }
      InstanceMapping im;
      if (minUv.x <= maxUv.x && minUv.y <= maxUv.y)
      {
        im.uvOffset = minUv;
        im.uvScale  = maxUv - minUv;
      }
      im.charts      = multi.chartsPerMesh[i];
      im.atlasWidth  = multi.atlasWidth;
      im.atlasHeight = multi.atlasHeight;
      mappings[i]    = im;
    }

    return mappings;
  }

} // namespace tools::uvunwrap
