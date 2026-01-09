#include <gtest/gtest.h>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace tools::uvunwrap;

static bool rectsOverlap(const glm::vec4& a, const glm::vec4& b)
{
  float ax1 = a.x;
  float ay1 = a.y;
  float ax2 = a.x + a.z;
  float ay2 = a.y + a.w;
  float bx1 = b.x;
  float by1 = b.y;
  float bx2 = b.x + b.z;
  float by2 = b.y + b.w;

  return !(ax2 <= bx1 || bx2 <= ax1 || ay2 <= by1 || by2 <= ay1);
}

TEST(UVUnwrap, MultiMeshNoOverlapAndPadding)
{
  MeshDecl meshA{};
  float    positionsA[12]  = {0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
  uint32_t indicesA[6]     = {0, 1, 2, 0, 2, 3};
  meshA.vertexPositionData = positionsA;
  meshA.indexData          = indicesA;
  meshA.vertexCount        = 4;
  meshA.indexCount         = 6;
  meshA.vertexStride       = sizeof(float) * 3;
  meshA.indexStride        = sizeof(uint32_t);

  MeshDecl meshB{};
  float    positionsB[12]  = {0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.25f, 0.25f, 0.0f, 0.0f, 0.25f, 0.0f};
  uint32_t indicesB[6]     = {0, 1, 2, 0, 2, 3};
  meshB.vertexPositionData = positionsB;
  meshB.indexData          = indicesB;
  meshB.vertexCount        = 4;
  meshB.indexCount         = 6;
  meshB.vertexStride       = sizeof(float) * 3;
  meshB.indexStride        = sizeof(uint32_t);

  int      paddingPx  = 4;
  uint32_t resolution = 128; // fixed atlas for deterministic checks

  std::vector<MeshDecl> meshes = {meshA, meshB};
  auto                  multi  = generateAtlasForMeshes(meshes, paddingPx, resolution);
  ASSERT_GT(multi.atlasWidth, 0);
  ASSERT_EQ(multi.chartsPerMesh.size(), 2);

  // Convert charts to pixel-space rects and ensure no overlap and padding respected
  for (size_t i = 0; i < multi.chartsPerMesh.size(); ++i)
  {
    for (size_t j = i + 1; j < multi.chartsPerMesh.size(); ++j)
    {
      for (const auto& a : multi.chartsPerMesh[i])
      {
        for (const auto& b : multi.chartsPerMesh[j])
        {
          glm::vec4 rectApx = glm::vec4(a.rect.x * multi.atlasWidth, a.rect.y * multi.atlasHeight, a.rect.z * multi.atlasWidth, a.rect.w * multi.atlasHeight);
          glm::vec4 rectBpx = glm::vec4(b.rect.x * multi.atlasWidth, b.rect.y * multi.atlasHeight, b.rect.z * multi.atlasWidth, b.rect.w * multi.atlasHeight);

          // Expand rectA by paddingPx and verify it doesn't overlap rectB
          glm::vec4 rectAexp = glm::vec4(rectApx.x - paddingPx, rectApx.y - paddingPx, rectApx.z + 2 * paddingPx, rectApx.w + 2 * paddingPx);

          EXPECT_FALSE(rectsOverlap(rectAexp, rectBpx));
        }
      }
    }
  }
}
