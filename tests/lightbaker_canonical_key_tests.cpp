#include <gtest/gtest.h>

#include "ModelLib/Resources/Model.hpp"
#include "Tools/LightBaker/LightBaker.hpp"

using namespace engine;

TEST(LightBakerCanonicalKey, DeterministicAcrossCalls)
{
  Model::Builder builder;
  // create a simple triangle primitive
  Model::Vertex v0{};
  v0.position = {0.0f, 0.0f, 0.0f};
  Model::Vertex v1{};
  v1.position = {1.0f, 0.0f, 0.0f};
  Model::Vertex v2{};
  v2.position = {0.0f, 1.0f, 0.0f};
  builder.vertices.push_back(v0);
  builder.vertices.push_back(v1);
  builder.vertices.push_back(v2);

  // primitive at node 0, prim 0 starting at vertex 0 count 3
  std::string key                     = std::to_string(0) + "_" + std::to_string(0);
  builder.primitiveVertexOffsets[key] = 0;
  builder.primitiveVertexCounts[key]  = 3;

  std::string canonical1 = LightBaker::makeCanonicalPrimitiveKey(builder, "models/simple.gltf", 0, 0, 256);
  std::string canonical2 = LightBaker::makeCanonicalPrimitiveKey(builder, "models/simple.gltf", 0, 0, 256);
  EXPECT_EQ(canonical1, canonical2);

  std::string id1 = LightBaker::makeLightmapIdFromKey(canonical1);
  std::string id2 = LightBaker::makeLightmapIdFromKey(canonical2);
  EXPECT_EQ(id1, id2);
}

TEST(LightBakerCanonicalKey, ChangesWithGeometryOrResolution)
{
  Model::Builder builderA;
  Model::Vertex  a0{};
  a0.position = {0.0f, 0.0f, 0.0f};
  Model::Vertex a1{};
  a1.position = {1.0f, 0.0f, 0.0f};
  Model::Vertex a2{};
  a2.position = {0.0f, 1.0f, 0.0f};
  builderA.vertices.push_back(a0);
  builderA.vertices.push_back(a1);
  builderA.vertices.push_back(a2);
  std::string kA                      = std::to_string(0) + "_" + std::to_string(0);
  builderA.primitiveVertexOffsets[kA] = 0;
  builderA.primitiveVertexCounts[kA]  = 3;

  Model::Builder builderB = builderA;
  // modify a vertex slightly to change checksum
  builderB.vertices[1].position.x = 1.0001f;

  std::string canonicalA = LightBaker::makeCanonicalPrimitiveKey(builderA, "models/simple.gltf", 0, 0, 256);
  std::string canonicalB = LightBaker::makeCanonicalPrimitiveKey(builderB, "models/simple.gltf", 0, 0, 256);
  EXPECT_NE(canonicalA, canonicalB);

  std::string idA = LightBaker::makeLightmapIdFromKey(canonicalA);
  std::string idB = LightBaker::makeLightmapIdFromKey(canonicalB);
  EXPECT_NE(idA, idB);

  // Same geometry but different resolution should differ
  std::string canonicalA2048 = LightBaker::makeCanonicalPrimitiveKey(builderA, "models/simple.gltf", 0, 0, 2048);
  EXPECT_NE(canonicalA, canonicalA2048);
  EXPECT_NE(LightBaker::makeLightmapIdFromKey(canonicalA), LightBaker::makeLightmapIdFromKey(canonicalA2048));
}

TEST(LightBakerCanonicalKey, ChangesWithUVParameters)
{
  Model::Builder builder;
  Model::Vertex  v0{};
  v0.position = {0.0f, 0.0f, 0.0f};
  Model::Vertex v1{};
  v1.position = {1.0f, 0.0f, 0.0f};
  Model::Vertex v2{};
  v2.position = {0.0f, 1.0f, 0.0f};
  builder.vertices.push_back(v0);
  builder.vertices.push_back(v1);
  builder.vertices.push_back(v2);

  std::string key                     = std::to_string(0) + "_" + std::to_string(0);
  builder.primitiveVertexOffsets[key] = 0;
  builder.primitiveVertexCounts[key]  = 3;

  std::string base    = LightBaker::makeCanonicalPrimitiveKey(builder, "models/simple.gltf", 0, 0, 256);
  std::string altered = LightBaker::makeCanonicalPrimitiveKey(builder, "models/simple.gltf", 0, 0, 256, 1, 0.5, 0.5, 0.0, 0.0);
  EXPECT_NE(base, altered);

  std::string idBase = LightBaker::makeLightmapIdFromKey(base);
  std::string idAlt  = LightBaker::makeLightmapIdFromKey(altered);
  EXPECT_NE(idBase, idAlt);
}
