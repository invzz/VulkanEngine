#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

TEST(UVUnwrapCLI, GeneratesManifestFromInput)
{
  namespace fs = std::filesystem;
  fs::create_directories("assets/scenes/test");

  // Construct a simple mesh + instance (triangle)
  std::vector<float>    positions = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  std::vector<uint32_t> indices   = {0, 1, 2};

  tools::uvunwrap::MeshDecl mesh{};
  mesh.vertexPositionData = positions.data();
  mesh.vertexCount        = static_cast<uint32_t>(positions.size() / 3);
  mesh.vertexStride       = sizeof(float) * 3;
  mesh.indexData          = indices.data();
  mesh.indexCount         = static_cast<uint32_t>(indices.size());
  mesh.indexStride        = sizeof(uint32_t);

  std::vector<std::pair<tools::uvunwrap::MeshDecl, glm::mat4>> meshesWithTransform;
  meshesWithTransform.emplace_back(mesh, glm::mat4(1.0f));

  auto mappings = tools::uvunwrap::generateInstanceMappings(meshesWithTransform, /*paddingPx=*/4, /*resolution=*/0);
  ASSERT_EQ(mappings.size(), 1u);

  const auto& m = mappings[0];
  // Expect atlas sizes to be non-zero
  ASSERT_GT(m.atlasWidth, 0u);
  ASSERT_GT(m.atlasHeight, 0u);

  // Emit a JSON manifest compatible with previous CLI output for downstream consumers
  nlohmann::json out;
  out["lightmapBindings"]              = nlohmann::json::object();
  out["lightmapBindings"]["object_01"] = {{"uvScale", {m.uvScale.x, m.uvScale.y}}, {"uvOffset", {m.uvOffset.x, m.uvOffset.y}}, {"resolution", {m.atlasWidth, m.atlasHeight}}};

  std::ofstream ofs("assets/scenes/test/uv_unwrap_output.json");
  ofs << out.dump(2) << std::endl;
  ofs.close();

  std::ifstream got("assets/scenes/test/uv_unwrap_output.json");
  ASSERT_TRUE(got.good());
  nlohmann::json lm;
  got >> lm;
  ASSERT_TRUE(lm.contains("lightmapBindings"));
  ASSERT_TRUE(lm["lightmapBindings"].contains("object_01"));
}
