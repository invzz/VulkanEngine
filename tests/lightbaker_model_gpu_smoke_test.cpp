#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
// Run a small GPU bake of the Cube model to ensure the GPU path runs and produces output
TEST(LightBaker, ModelCLI_Smoke_GPU)
{
  const std::string modelPath  = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
  const std::string lightBaker = std::string(LIGHT_BAKER_PATH);

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_gpu_test";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / "out").generic_string();

  // Run GPU bake only (no CPU fallback) to validate GPU path is working
  const std::string cmdGpu = lightBaker + " --model " + modelPath + " --out " + outDir + " --resolution 8 --samples 1 --gpu --keep-exr";
  std::cout << "Invoking (GPU): " << cmdGpu << '\n';
  int ret = std::system(cmdGpu.c_str());
  ASSERT_EQ(ret, 0) << "LightBaker GPU bake failed (exit code " << ret << ") cmd: " << cmdGpu;

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
}
#else
GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif
#else
GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
