#include <gtest/gtest.h>

#include "Tools/LightmapBakerLib/Geometry.hpp"
#include "Tools/LightmapBakerLib/Scene.hpp"

using namespace LightmapBaker;
using namespace engine;

// Build a small synthetic builder with one mesh and two nodes pointing to it to test instance transform application
static engine::Model::Builder makeTestBuilder()
{
  engine::Model::Builder b;
  b.filePath = "test";

  // single triangle
  engine::Model::Vertex v0;
  v0.position = {0.0f, 0.0f, 0.0f};
  v0.normal   = {0, 0, 1};
  v0.uv       = {0, 0};
  engine::Model::Vertex v1;
  v1.position = {1.0f, 0.0f, 0.0f};
  v1.normal   = {0, 0, 1};
  v1.uv       = {1, 0};
  engine::Model::Vertex v2;
  v2.position = {0.0f, 1.0f, 0.0f};
  v2.normal   = {0, 0, 1};
  v2.uv       = {0, 1};

  b.vertices.push_back(v0);
  b.vertices.push_back(v1);
  b.vertices.push_back(v2);

  b.indices.push_back(0);
  b.indices.push_back(1);
  b.indices.push_back(2);

  engine::Model::SubMesh sm;
  sm.indexOffset = 0;
  sm.indexCount  = 3;
  sm.materialId  = 0;
  b.subMeshes.push_back(sm);

  // two nodes referencing the same mesh (mesh index 0)
  engine::Model::Node n0;
  n0.mesh        = 0;
  n0.translation = glm::vec3(0, 0, 0);
  b.nodes.push_back(n0);

  engine::Model::Node n1;
  n1.mesh        = 0;
  n1.translation = glm::vec3(10, 0, 0);
  b.nodes.push_back(n1);

  return b;
}

TEST(LightmapGeometry, AppliesNodeTransforms)
{
  auto      b = makeTestBuilder();
  glm::mat4 identity(1.0f);
  auto      tris = collectTrianglesFromBuilder(b, identity);
  // Expect 2 nodes * 1 triangle = 2 triangles
  ASSERT_EQ(tris.size(), 2);
  // First triangle should have positions near original
  EXPECT_NEAR(tris[0].p0.x, 0.0f, 1e-6f);
  EXPECT_NEAR(tris[1].p0.x, 10.0f, 1e-6f);
}

TEST(LightmapGeometry, CollectsTrianglesFromScene_MultiSubmesh)
{
  // Build a builder with two triangles in separate submeshes
  engine::Model::Builder b;
  b.filePath = "test_multi";

  for (int i = 0; i < 6; ++i)
  {
    engine::Model::Vertex v;
    v.position = glm::vec3(float(i), 0.0f, 0.0f);
    v.normal   = glm::vec3(0, 0, 1);
    v.uv       = glm::vec2(0.0f);
    b.vertices.push_back(v);
  }

  // two triangles (0,1,2) and (3,4,5)
  b.indices = {0, 1, 2, 3, 4, 5};

  engine::Model::SubMesh sm0;
  sm0.indexOffset = 0;
  sm0.indexCount  = 3;
  sm0.materialId  = 0;
  b.subMeshes.push_back(sm0);

  engine::Model::SubMesh sm1;
  sm1.indexOffset = 3;
  sm1.indexCount  = 3;
  sm1.materialId  = 1;
  b.subMeshes.push_back(sm1);

  LightmapBaker::Scene  s;
  LightmapBaker::Object obj;
  obj.id        = "multi";
  obj.name      = "multi";
  obj.modelPath = std::string("multi_path");
  s.objects.push_back(obj);

  auto loader = [&](const std::string& p, engine::Model::Builder& out) -> bool {
    if (p == "multi_path")
    {
      out = b;
      return true;
    }
    return false;
  };

  auto tris = LightmapBaker::collectTrianglesFromScene(s, loader);
  ASSERT_EQ(tris.size(), 2);
}

TEST(LightmapGeometry, SceneInstancesApplyTransforms)
{
  auto b = makeTestBuilder();

  LightmapBaker::Scene  s;
  LightmapBaker::Object obj;
  obj.id        = "inst";
  obj.name      = "inst";
  obj.modelPath = std::string("inst_path");
  s.objects.push_back(obj);

  auto loader = [&](const std::string& p, engine::Model::Builder& out) -> bool {
    if (p == "inst_path")
    {
      out = b;
      return true;
    }
    return false;
  };

  auto tris = LightmapBaker::collectTrianglesFromScene(s, loader);
  ASSERT_EQ(tris.size(), 2);
  EXPECT_NEAR(tris[0].p0.x, 0.0f, 1e-6f);
  EXPECT_NEAR(tris[1].p0.x, 10.0f, 1e-6f);
}
