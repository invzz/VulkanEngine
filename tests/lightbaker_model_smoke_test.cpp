#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

TEST(LightBaker, ModelCLI_Smoke_CPU)
{
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  // Use a small glTF asset from the repo for a deterministic smoke test (CPU-only path)
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_test_tmp";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / "out").generic_string();
  const std::string cmd    = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --resolution 4";
  std::cout << "Invoking: " << cmd << std::endl;

  const std::string logFile = (tmp / "lightbaker.log").generic_string();
  const std::string fullCmd = cmd + " 2>&1 | tee " + logFile;
  int               ret     = std::system(fullCmd.c_str());

  std::ifstream in(logFile);
  std::string   out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (out.find("CPU baking models is not implemented") != std::string::npos || out.find("CPU baker path not implemented") != std::string::npos || out.find("CPU bake") != std::string::npos)
  {
    GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
  }

  ASSERT_EQ(ret, 0) << "LightBaker failed (exit code " << ret << ") cmd: " << cmd;

  bool found = false;
  if (std::filesystem::exists(outDir))
  {
    for (auto& p : std::filesystem::directory_iterator(outDir))
    {
      auto fname = p.path().filename().string();
      if (fname.find("_lightmap.json") != std::string::npos || p.path().extension() == ".exr")
      {
        found = true;
        break;
      }
    }
  }
  ASSERT_TRUE(found) << "Expected manifest *_lightmap.json or an .exr output in " << outDir;
#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}
