#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

TEST(LightBaker, SceneLights_CLIIncludesBakedLightsInManifest)
{
#ifdef LIGHT_BAKER_PATH
  if (!std::getenv("RUN_HARDWARE_TESTS"))
  {
    GTEST_SKIP() << "Skipping hardware-dependent LightBaker scene test; set RUN_HARDWARE_TESTS=1 to enable";
  }

  const std::string name      = "Sponza";
  const std::string modelFile = (std::string(MODEL_PATH) + "/glTF/" + name + "/glTF/" + name + ".gltf");
  const std::string outDir    = std::string(LIGHTMAP_PATH) + "test/lightbaker_scene_test";
  std::filesystem::create_directories(outDir);

  // Ensure demo scene exists in test assets
  const std::string scenePath = std::string(SCENE_PATH) + std::string("test/demo_scene_bake.json");
  ASSERT_TRUE(std::filesystem::exists(scenePath)) << "Expected demo scene for test: " << scenePath;

  // Invoke LightBaker in scene mode via the CLI
  std::string cmd = LIGHT_BAKER_BIN + " --scene " + scenePath + " --out " + outDir + " --resolution 16";
  std::cout << "Invoking: " << cmd << std::endl;

  int ret = std::system(cmd.c_str());
  ASSERT_EQ(ret, 0) << "LightBaker failed (exit code " << ret << ")";

  // Verify manifest contains bakedLights and that sun_01 is present
  std::filesystem::path manifestPath = std::filesystem::path(outDir) / (std::filesystem::path(modelFile).stem().string() + std::string("_lightmap.json"));
  ASSERT_TRUE(std::filesystem::exists(manifestPath)) << "Expected manifest: " << manifestPath.generic_string();

  std::ifstream manifestIn(manifestPath);
  std::string   manifestStr((std::istreambuf_iterator<char>(manifestIn)), std::istreambuf_iterator<char>());
  ASSERT_NE(manifestStr.find("\"bakedLights\""), std::string::npos) << "Manifest missing bakedLights field: " << manifestPath.generic_string();

  // sun_01 should be baked (present)
  EXPECT_NE(manifestStr.find("sun_01"), std::string::npos) << "Expected sun_01 in bakedLights";
  // lamp_01 should NOT be baked (absent)
  EXPECT_EQ(manifestStr.find("lamp_01"), std::string::npos) << "Did not expect lamp_01 to be in bakedLights";

  // Additional check: run with instance mode on a scene that contains an object id
  {
    std::string   instScenePath = std::string(SCENE_PATH) + std::string("test/demo_scene_with_instance.json");
    std::ofstream out(instScenePath);
    out << R"({ "objects": [ { "id": "inst_01", "name": "inst_01", "transform": { "translation": [1.0, 2.0, 3.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] } } ] })";
    out.close();

    std::string cmdInst = LIGHT_BAKER_BIN + " --scene " + instScenePath + " --out " + outDir + " --resolution 16 --instance inst_01";
    std::cout << "Invoking (instance): " << cmdInst << std::endl;
    int retInst = std::system(cmdInst.c_str());
    ASSERT_EQ(retInst, 0) << "LightBaker failed in instance mode (exit code " << retInst << ")";

    std::filesystem::path manifestPathInst = std::filesystem::path(outDir) / (std::filesystem::path(modelFile).stem().string() + std::string("_inst_01_lightmap.json"));
    ASSERT_TRUE(std::filesystem::exists(manifestPathInst)) << "Expected manifest for instance: " << manifestPathInst.generic_string();

    std::ifstream manifestInInst(manifestPathInst);
    std::string   manifestStrInst((std::istreambuf_iterator<char>(manifestInInst)), std::istreambuf_iterator<char>());
    EXPECT_NE(manifestStrInst.find("\"instanceId\""), std::string::npos) << "Manifest missing instanceId field";
  }
#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined by the build system";
#endif
}
