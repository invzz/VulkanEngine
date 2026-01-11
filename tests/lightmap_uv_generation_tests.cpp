#include <gtest/gtest.h>

#include "Engine/Resources/Model.hpp"
#include "Tools/LightmapBakerLib/Geometry.hpp"
#include "Tools/UVUnwrap/UVUnwrap.hpp"

using namespace LightmapBaker;
using namespace engine;

TEST(LightmapBaker_UV, GenerateInstanceUVsForNode_SimpleTriangle)
{
  Model::Builder builder;
  // simple triangle
  Model::Vertex va;
  va.position      = glm::vec3(0.0f, 0.0f, 0.0f);
  va.normal        = glm::vec3(0.0f, 0.0f, 1.0f);
  va.uv            = glm::vec2(0.0f, 0.0f);
  Model::Vertex vb = va;
  vb.position      = glm::vec3(1.0f, 0.0f, 0.0f);
  vb.uv            = glm::vec2(1.0f, 0.0f);
  Model::Vertex vc = va;
  vc.position      = glm::vec3(0.0f, 1.0f, 0.0f);
  vc.uv            = glm::vec2(0.0f, 1.0f);

  builder.vertices.push_back(va);
  builder.vertices.push_back(vb);
  builder.vertices.push_back(vc);

  builder.indices = {0, 1, 2};

  Model::SubMesh sub;
  sub.indexOffset = 0;
  sub.indexCount  = 3;
  sub.materialId  = 0;
  builder.subMeshes.push_back(sub);

  Model::Node node;
  node.mesh = 0;
  builder.nodes.push_back(node);

  auto per = generatePerVertexUVsForNode(builder, 0, glm::mat4(1.0f), 4, 64);
  // Ensure UVs were produced for the three original vertices
  ASSERT_EQ(per.uvPerVertex.size(), builder.vertices.size());
  int usedCount = 0;
  for (size_t i = 0; i < per.used.size(); ++i)
    if (per.used[i]) ++usedCount;
  EXPECT_EQ(usedCount, 3);

  // Atlas metadata should be valid
  ASSERT_GT(per.atlasResult.uv1.size(), 0u);
  EXPECT_GT(per.atlasResult.uvScale.x, 0.0f);
  EXPECT_GT(per.atlasResult.uvScale.y, 0.0f);
  EXPECT_GE(per.atlasResult.uvOffset.x, 0.0f);
  EXPECT_GE(per.atlasResult.uvOffset.y, 0.0f);
  EXPECT_LE(per.atlasResult.uvOffset.x + per.atlasResult.uvScale.x, 1.0f + 1e-6f);
  EXPECT_LE(per.atlasResult.uvOffset.y + per.atlasResult.uvScale.y, 1.0f + 1e-6f);
}
