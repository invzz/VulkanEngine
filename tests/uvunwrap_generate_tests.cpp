#include <gtest/gtest.h>
#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace tools::uvunwrap;

TEST(UVUnwrap, GenerateInstanceUVs_Stub)
{
  MeshDecl mesh{};
  // simple triangle
  float positions[9] = {0.0f,0.0f,0.0f, 1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f};
  uint32_t indices[3] = {0,1,2};
  mesh.vertexPositionData = positions;
  mesh.indexData = indices;
  mesh.vertexCount = 3;
  mesh.indexCount = 3;
  mesh.vertexStride = sizeof(float)*3;
  mesh.indexStride = sizeof(uint32_t);

  Result r = generateInstanceUVs(mesh, glm::mat4(1.0f), 4, 512);
  ASSERT_EQ(r.uv1.size(), (size_t)mesh.vertexCount);
  EXPECT_EQ(r.uvScale, glm::vec2(1.0f,1.0f));
  EXPECT_EQ(r.uvOffset, glm::vec2(0.0f,0.0f));
}
