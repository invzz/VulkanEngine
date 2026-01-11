#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Scene/LightmapManifest.hpp"

using namespace engine::scene;

TEST(LightmapManifest, RejectLegacyBindings)
{
  const std::string     testPath = "assets/scenes/test/test_lightmaps_legacy.json";
  std::filesystem::path p(testPath);
  std::filesystem::create_directories(p.parent_path());
  std::ofstream out(testPath);
  ASSERT_TRUE(out.good());
  // Old legacy binding format (flat lightmapBindings) should be rejected by the parser
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

  // Should return false as legacy bindings are no longer supported
  bool ok = parseSceneLightmapManifest(testPath, infos, binds);
  EXPECT_FALSE(ok);

  std::filesystem::remove(testPath);
}