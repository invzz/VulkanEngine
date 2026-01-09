#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace tools::uvunwrap {

  struct MeshDecl
  {
    const void* vertexPositionData = nullptr; // float3
    const void* vertexNormalData   = nullptr; // float3 (optional)
    const void* vertexUvData       = nullptr; // float2 (optional)
    const void* indexData          = nullptr; // uint32 or uint16
    uint32_t    vertexCount        = 0;
    uint32_t    indexCount         = 0;
    uint32_t    vertexStride       = 0; // bytes
    uint32_t    indexStride        = 0; // bytes (2 or 4)
  };

  struct ChartRect
  {
    uint32_t  atlasIndex;
    glm::vec4 rect; // x,y,width,height in texel space or normalized depending on usage
  };

  struct Result
  {
    std::vector<glm::vec2>                uv1; // per-vertex UV1
    glm::vec2                             uvScale{1.0f, 1.0f};
    glm::vec2                             uvOffset{0.0f, 0.0f};
    std::optional<std::vector<ChartRect>> charts; // optional chart/rect metadata
  };

  // Generate per-instance UV1 for the provided mesh geometry.
  // - mesh: geometry to unwrap (vertex positions required)
  // - instanceTransform: world transform for texel-density computations (optional)
  // - paddingPx: padding in texels to reserve around charts
  // - resolution: target atlas resolution or 0 to use texelsPerUnit
  // Returns a Result containing UV1 and atlas placement metadata only.
  Result generateInstanceUVs(const MeshDecl& mesh, const glm::mat4& instanceTransform, int paddingPx, uint32_t resolution);

} // namespace tools::uvunwrap
