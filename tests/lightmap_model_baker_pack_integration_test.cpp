#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef MODEL_LIGHT_BAKER_PATH
static const std::string MODEL_LIGHT_BAKER_BIN = MODEL_LIGHT_BAKER_PATH;
#endif

TEST(ModelLightBaker, PackToVTEX_CLIProducesVTEX)
{
#ifdef MODEL_LIGHT_BAKER_PATH
  // Use a small resolution to keep test fast
  const std::string name = "Sponza";

  const std::string modelFile = (std::string(MODEL_PATH) + "/glTF/" + name + "/glTF/" + name + ".gltf");
  const std::string outDir    = std::string(LIGHTMAP_PATH) + "test/model_baker_test";

  // Ensure output dir exists
  std::filesystem::create_directories(outDir);

  // Create sentinel file so ModelLightBaker detects packing request reliably (legacy)
  std::ofstream(std::filesystem::path(outDir) / "MODEL_LIGHT_BAKER_PACK_TO_VTEX").close();

  // Ensure packing is always triggered in CI by setting the env var for the child process
  std::string cmd = MODEL_LIGHT_BAKER_BIN + " " + modelFile + " " + outDir + " --res 16 --pack-to-vtex";
  std::cout << "Invoking: " << cmd << std::endl;

  // Set environment variable in this process so std::system child sees it reliably
  setenv("MODEL_LIGHT_BAKER_PACK_TO_VTEX", "1", 1);

  int ret = std::system(cmd.c_str());
  ASSERT_EQ(ret, 0) << "ModelLightBaker failed (exit code " << ret << ")";

  // Expect vtex file to exist next to the exr-derived name
  std::filesystem::path vtexPath = std::filesystem::path(outDir) / (std::filesystem::path(modelFile).stem().string() + std::string("_lightmap.vtex"));
  ASSERT_TRUE(std::filesystem::exists(vtexPath)) << "Expected VTEX produced at: " << vtexPath.generic_string();
#else
  GTEST_SKIP() << "MODEL_LIGHT_BAKER_PATH macro not defined by the build system";
#endif
}
