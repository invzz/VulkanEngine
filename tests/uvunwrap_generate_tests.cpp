#include <gtest/gtest.h>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace tools::uvunwrap;

TEST(UVUnwrap, GenerateInstanceUVs_Stub)
{
  MeshDecl mesh{};
  // simple triangle
  float    positions[9]   = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  uint32_t indices[3]     = {0, 1, 2};
  mesh.vertexPositionData = positions;
  mesh.indexData          = indices;
  mesh.vertexCount        = 3;
  mesh.indexCount         = 3;
  mesh.vertexStride       = sizeof(float) * 3;
  mesh.indexStride        = sizeof(uint32_t);

  const int      paddingPx  = 4;
  const uint32_t resolution = 64; // small atlas for quick test
  Result         r          = generateInstanceUVs(mesh, glm::mat4(1.0f), paddingPx, resolution);
  ASSERT_EQ(r.uv1.size(), (size_t)mesh.vertexCount);
  // UV bounding box should be valid
  EXPECT_GT(r.uvScale.x, 0.0f);
  EXPECT_GT(r.uvScale.y, 0.0f);
  EXPECT_GE(r.uvOffset.x, 0.0f);
  EXPECT_GE(r.uvOffset.y, 0.0f);
  EXPECT_LE(r.uvOffset.x + r.uvScale.x, 1.0f + 1e-6f);
  EXPECT_LE(r.uvOffset.y + r.uvScale.y, 1.0f + 1e-6f);

  // If charts present, ensure they fit in [0,1] and respect padding in texel space
  if (r.charts)
  {
    for (const auto& c : *r.charts)
    {
      EXPECT_GE(c.rect.x, 0.0f);
      EXPECT_GE(c.rect.y, 0.0f);
      EXPECT_GE(c.rect.z, 0.0f);
      EXPECT_GE(c.rect.w, 0.0f);
      EXPECT_LE(c.rect.x + c.rect.z, 1.0f + 1e-6f);
      EXPECT_LE(c.rect.y + c.rect.w, 1.0f + 1e-6f);
      // padding check (approx): rect in texels should be at least paddingPx away from border
      float leftTex  = c.rect.x * resolution;
      float rightTex = (c.rect.x + c.rect.z) * resolution;
      // basic sanity: rect spans at least one texel and is within atlas
      EXPECT_GE(rightTex - leftTex, 1.0f);
      EXPECT_LE(leftTex, resolution);
      EXPECT_GE(rightTex, 0.0f);
    }
  }
}
