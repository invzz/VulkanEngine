#include "Tools/UVUnwrap/UVUnwrap.hpp"

#include <xatlas.h>

namespace tools::uvunwrap {

  Result generateInstanceUVs(const MeshDecl& mesh, const glm::mat4& /*instanceTransform*/, int /*paddingPx*/, uint32_t /*resolution*/)
  {
    Result r;
    // Minimal stub: produce zeroed UVs matching vertex count
    r.uv1.resize(mesh.vertexCount, glm::vec2(0.0f, 0.0f));
    r.uvScale  = glm::vec2(1.0f, 1.0f);
    r.uvOffset = glm::vec2(0.0f, 0.0f);
    return r;
  }

} // namespace tools::uvunwrap
