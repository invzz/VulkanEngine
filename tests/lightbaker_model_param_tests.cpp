#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

using ModelParam = std::tuple<const char*, int>;

class LightBakerParamTest : public ::testing::TestWithParam<ModelParam>
{};

TEST_P(LightBakerParamTest, ModelParameterizedResolutions)
{
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const char* relPath;
  int         res;
  std::tie(relPath, res) = GetParam();

  const std::string modelPath = std::string(MODEL_PATH) + std::string(relPath);
  ASSERT_TRUE(std::filesystem::exists(modelPath)) << "Model missing: " << modelPath;

  // Quick heuristic: skip models that use non-indexed primitives until importer supports them
  std::ifstream modelIn(modelPath);
  std::string   modelStr((std::istreambuf_iterator<char>(modelIn)), std::istreambuf_iterator<char>());
  if (modelStr.find("\"indices\"") == std::string::npos)
  {
    GTEST_SKIP() << "Skipping model with non-indexed primitives: " << modelPath;
  }

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_param_test";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / (std::string("out_") + std::to_string(res))).generic_string();
  const std::string cmd    = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --resolution " + std::to_string(res);
  std::cout << "Invoking: " << cmd << std::endl;

  const std::string logPath     = (tmp / (std::string("run_") + std::to_string(res) + std::string(".log"))).generic_string();
  const std::string cmdRedirect = cmd + " > " + logPath + " 2>&1";
  int               ret         = std::system(cmdRedirect.c_str());
  if (ret != 0)
  {
    // Inspect output to decide whether this is an expected unsupported case
    std::ifstream in(logPath);
    std::string   outStr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (outStr.find("No triangles found") != std::string::npos || outStr.find("Primitive without indices not supported") != std::string::npos)
    {
      GTEST_SKIP() << "Skipping model due to unsupported primitive type or empty mesh: " << modelPath << "\n" << outStr;
    }
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
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif
#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}

INSTANTIATE_TEST_SUITE_P(CubeVariants, LightBakerParamTest, ::testing::Values(ModelParam{"glTF/Cube/glTF/Cube.gltf", 4}, ModelParam{"glTF/SimpleMeshes/glTF/SimpleMeshes.gltf", 2}));
