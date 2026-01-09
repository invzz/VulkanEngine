#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace tools::uvunwrap;

TEST(UVUnwrap, ScaleAffectsAtlasSize)
{
  MeshDecl mesh{};
  // simple quad (two triangles)
  float positions[12] = {
          0.0f,
          0.0f,
          0.0f, // 0
          1.0f,
          0.0f,
          0.0f, // 1
          1.0f,
          1.0f,
          0.0f, // 2
          0.0f,
          1.0f,
          0.0f // 3
  };
  uint32_t indices[6]     = {0, 1, 2, 0, 2, 3};
  mesh.vertexPositionData = positions;
  mesh.indexData          = indices;
  mesh.vertexCount        = 4;
  mesh.indexCount         = 6;
  mesh.vertexStride       = sizeof(float) * 3;
  mesh.indexStride        = sizeof(uint32_t);

  const int paddingPx = 2;
  // Small scale: 0.5x
  glm::mat4 smallTransform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
  Result    rSmall         = generateInstanceUVs(mesh, smallTransform, paddingPx, 0);
  ASSERT_TRUE(rSmall.charts.has_value());
  uint64_t areaSmall = 0;
  for (const auto& c : *rSmall.charts)
  {
    float area = c.rect.z * c.rect.w * static_cast<float>(rSmall.atlasWidth) * static_cast<float>(rSmall.atlasHeight);
    areaSmall += static_cast<uint64_t>(std::round(area));
  }

  // Large scale: 2.0x
  glm::mat4 largeTransform = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  Result    rLarge         = generateInstanceUVs(mesh, largeTransform, paddingPx, 0);
  ASSERT_TRUE(rLarge.charts.has_value());
  uint64_t areaLarge = 0;
  for (const auto& c : *rLarge.charts)
  {
    float area = c.rect.z * c.rect.w * static_cast<float>(rLarge.atlasWidth) * static_cast<float>(rLarge.atlasHeight);
    areaLarge += static_cast<uint64_t>(std::round(area));
  }

  // Expect larger scale -> larger chart area in texels
  EXPECT_GT(areaLarge, areaSmall);
}
