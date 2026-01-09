#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace tools::uvunwrap;

TEST(UVUnwrap, InstanceMappingsEmitUVScaleOffset)
{
  MeshDecl mesh{};
  float    positions[9]   = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  uint32_t indices[3]     = {0, 1, 2};
  mesh.vertexPositionData = positions;
  mesh.indexData          = indices;
  mesh.vertexCount        = 3;
  mesh.indexCount         = 3;
  mesh.vertexStride       = sizeof(float) * 3;
  mesh.indexStride        = sizeof(uint32_t);

  std::vector<std::pair<MeshDecl, glm::mat4>> instances;
  instances.push_back({mesh, glm::scale(glm::mat4(1.0f), glm::vec3(0.5f))});
  instances.push_back({mesh, glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))});

  int      paddingPx  = 2;
  uint32_t resolution = 128;

  auto mappings = generateInstanceMappings(instances, paddingPx, resolution);
  ASSERT_EQ(mappings.size(), 2);
  for (const auto& m : mappings)
  {
    EXPECT_GT(m.uvScale.x, 0.0f);
    EXPECT_GT(m.uvScale.y, 0.0f);
    EXPECT_GE(m.uvOffset.x, 0.0f);
    EXPECT_GE(m.uvOffset.y, 0.0f);
    EXPECT_GT(m.atlasWidth, 0u);
    EXPECT_GT(m.atlasHeight, 0u);
  }

  // Expect larger instance -> larger uvScale in texels (approximately)
  uint32_t area0 = static_cast<uint32_t>(std::round(mappings[0].uvScale.x * mappings[0].uvScale.y * mappings[0].atlasWidth * mappings[0].atlasHeight));
  uint32_t area1 = static_cast<uint32_t>(std::round(mappings[1].uvScale.x * mappings[1].uvScale.y * mappings[1].atlasWidth * mappings[1].atlasHeight));
  EXPECT_GT(area1, area0);
}
