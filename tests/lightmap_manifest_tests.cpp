#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Scene/LightmapManifest.hpp"

using namespace engine::scene;

TEST(LightmapManifest, ParseSimpleManifest)
{
  const std::string testPath = "assets/scenes/test_lightmaps.json";
  // Ensure parent directory exists then write a small manifest
  std::filesystem::path p(testPath);
  std::filesystem::create_directories(p.parent_path());
  std::ofstream out(testPath);
  ASSERT_TRUE(out.good());
  out << R"({
    "version": 1,
    "lightmapBindings": {
      "object_01": { "lightmapId": "lm_000", "uvChannel": 1, "uvScale": [0.25, 0.25], "uvOffset": [0.5, 0.0] }
    },
    "lightmaps": [ { "id": "lm_000", "file": "lightmaps/lm_000_atlas.vtex", "format": "vtex", "resolution": [2048, 2048], "usage": "Lightmap" } ]
  })";
  out.close();

  std::unordered_map<std::string, LightmapInfo>    infos;
  std::unordered_map<std::string, LightmapBinding> binds;

  bool ok = parseSceneLightmapManifest(testPath, infos, binds);
  EXPECT_TRUE(ok);
  EXPECT_EQ(infos.size(), 1u);
  EXPECT_EQ(binds.size(), 1u);

  auto it = infos.find("lm_000");
  ASSERT_NE(it, infos.end());
  EXPECT_EQ(it->second.file, "lightmaps/lm_000_atlas.vtex");
  EXPECT_EQ(it->second.resolution[0], 2048);

  auto bt = binds.find("object_01");
  ASSERT_NE(bt, binds.end());
  EXPECT_EQ(bt->second.lightmapId, "lm_000");
  EXPECT_FLOAT_EQ(bt->second.uvScale.x, 0.25f);

  // cleanup
  std::filesystem::remove(testPath);
}
