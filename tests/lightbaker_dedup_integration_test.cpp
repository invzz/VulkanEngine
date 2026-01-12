#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

TEST(LightBaker, Scene_Dedup_RepeatedInstances)
{
#ifdef LIGHT_BAKER_PATH
  if (!std::getenv("RUN_HARDWARE_TESTS"))
  {
    GTEST_SKIP() << "Skipping hardware-dependent LightBaker dedup test; set RUN_HARDWARE_TESTS=1 to enable";
  }

  const std::filesystem::path tmp = std::filesystem::temp_directory_path() / "lightbaker_dedup_test_tmp";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string           sceneStem = "dedup_scene";
  const std::filesystem::path scenePath = tmp / (sceneStem + std::string(".json"));
  const std::string           modelRel  = std::string(MODEL_PATH) + std::string("/glTF/Cube/glTF/Cube.gltf");

  // Two objects referencing the same model but different transforms
  std::ofstream out(scenePath);
  out << R"({ "objects": [
    { "id": "instA", "name": "instA", "modelPath": ")"
      << modelRel << R"(", "transform": { "translation": [0.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] } },
    { "id": "instB", "name": "instB", "modelPath": ")"
      << modelRel << R"(", "transform": { "translation": [2.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] } }
  ] })";
  out.close();

  const std::filesystem::path outDir = tmp / "out";
  std::filesystem::create_directories(outDir);

  std::string cmd = LIGHT_BAKER_BIN + " --scene " + scenePath.string() + " --out " + outDir.string() + " --resolution 8 --auto-uv --pack-to-vtex";
  std::cout << "Invoking: " << cmd << std::endl;
  int ret = std::system(cmd.c_str());
  ASSERT_EQ(ret, 0) << "LightBaker failed (exit code " << ret << ")";

  std::filesystem::path manifestPath = outDir / (sceneStem + std::string("_lightmap.json"));
  ASSERT_TRUE(std::filesystem::exists(manifestPath)) << "Expected manifest: " << manifestPath.generic_string();

  std::ifstream manifestIn(manifestPath);
  ASSERT_TRUE(manifestIn) << "Failed to open manifest: " << manifestPath.generic_string();
  nlohmann::json manifestJson;
  manifestIn >> manifestJson;

  ASSERT_TRUE(manifestJson.contains("lightmaps")) << "Manifest missing 'lightmaps'";
  auto lightmaps = manifestJson["lightmaps"];
  ASSERT_EQ(lightmaps.size(), 1) << "Expected exactly one produced lightmap due to deduplication";

  ASSERT_TRUE(manifestJson.contains("lightmapBindings"));
  auto bindings = manifestJson["lightmapBindings"];
  ASSERT_TRUE(bindings.contains("instA"));
  ASSERT_TRUE(bindings.contains("instB"));

  auto meshA = bindings["instA"]["meshes"];
  auto meshB = bindings["instB"]["meshes"];
  ASSERT_GT(meshA.size(), 0u);
  ASSERT_GT(meshB.size(), 0u);

  std::string lmA = meshA[0]["lightmapId"].get<std::string>();
  std::string lmB = meshB[0]["lightmapId"].get<std::string>();
  EXPECT_EQ(lmA, lmB) << "Both instances should reference the same lightmap id";

  // The lightmap file should exist under outDir/lightmaps/<sceneStem>/
  std::string           lmFile = lightmaps[0]["file"].get<std::string>();
  std::filesystem::path lmPath = outDir / lmFile;
  ASSERT_TRUE(std::filesystem::exists(lmPath)) << "Expected VTEX file: " << lmPath.generic_string();

  // Ensure there's only one vtex file in that directory
  std::filesystem::path lmDir     = outDir / "lightmaps" / sceneStem;
  size_t                vtexCount = 0;
  for (auto& e : std::filesystem::directory_iterator(lmDir))
    if (e.path().extension() == ".vtex") ++vtexCount;
  EXPECT_EQ(vtexCount, 1u) << "Expected a single VTEX file produced for deduped primitives";
#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined by the build system";
#endif
}
