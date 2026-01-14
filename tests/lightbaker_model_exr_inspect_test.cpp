#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

TEST(LightBaker, ModelEXR_Inspect)
{
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  // InspectEXR binary path (built by project)
  const std::filesystem::path inspectBin = std::filesystem::path(TOOL_PATH) / "InspectEXR";
  if (!std::filesystem::exists(inspectBin))
  {
    GTEST_SKIP() << "InspectEXR binary not found at " << inspectBin << "; build it first with xmake build InspectEXR";
  }

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_model_exr_inspect";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / "out").generic_string();
  const std::string cmd    = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --resolution 8 --gpu";
  std::cout << "Invoking: " << cmd << '\n';

  const std::string logFile = (tmp / "lightbaker.log").generic_string();
  const std::string fullCmd = cmd + " 2>&1 | tee " + logFile;
  int               ret     = std::system(fullCmd.c_str());

  std::ifstream inlog(logFile);
  std::string   out((std::istreambuf_iterator<char>(inlog)), std::istreambuf_iterator<char>());
  if (out.find("CPU baking models is not implemented") != std::string::npos || out.find("CPU baker path not implemented") != std::string::npos || out.find("CPU bake") != std::string::npos)
  {
    GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
  }

  ASSERT_EQ(ret, 0) << "LightBaker failed (exit code " << ret << ") cmd: " << cmd << "\nLog:\n" << out;

  // Find produced .exr file
  std::filesystem::path exrPath;
  if (std::filesystem::exists(outDir))
  {
    for (auto& p : std::filesystem::directory_iterator(outDir))
    {
      if (p.path().extension() == ".exr")
      {
        exrPath = p.path();
        break;
      }
    }
  }
  ASSERT_FALSE(exrPath.empty()) << "No .exr output found in " << outDir << " (see " << logFile << ")";

  // Run InspectEXR on the produced file
  const std::string inspectLog = (tmp / "inspect.log").generic_string();
  const std::string inspectCmd = inspectBin.generic_string() + " " + exrPath.generic_string() + " 3 > " + inspectLog + " 2>&1";
  std::cout << "Invoking InspectEXR: " << inspectCmd << '\n';
  int inspectRet = std::system(inspectCmd.c_str());
  ASSERT_EQ(inspectRet, 0) << "InspectEXR failed (exit code " << inspectRet << ") cmd: " << inspectCmd;

  std::ifstream inspectIn(inspectLog);
  std::string   inspectOut((std::istreambuf_iterator<char>(inspectIn)), std::istreambuf_iterator<char>());
  std::cout << "InspectEXR output:\n" << inspectOut << std::endl;

  // Parse stats line: stats: min=... max=... mean=...
  std::regex  statsRx("stats:\\s*min=([eE0-9+.-]+)\\s*max=([eE0-9+.-]+)\\s*mean=([eE0-9+.-]+)");
  std::smatch statsMatch;
  ASSERT_TRUE(std::regex_search(inspectOut, statsMatch, statsRx)) << "InspectEXR did not print stats line";

  double mn   = std::stod(statsMatch[1].str());
  double mx   = std::stod(statsMatch[2].str());
  double mean = std::stod(statsMatch[3].str());

  // Basic sanity checks
  ASSERT_LE(mn, mx) << "min should be <= max";
  ASSERT_GE(mean, mn - 1e-6) << "mean should be >= min";
  ASSERT_LE(mean, mx + 1e-6) << "mean should be <= max";

  // Ensure center crop lines exist and values lie between min and max
  // center crop prints lines like: "0: [r, g, b]"
  std::regex  cropRx("\\d+: \\[([eE0-9+.-]+), ([eE0-9+.-]+), ([eE0-9+.-]+)\\]");
  std::smatch cropMatch;
  ASSERT_TRUE(std::regex_search(inspectOut, cropMatch, cropRx)) << "InspectEXR did not print center crop values";

  double cr = std::stod(cropMatch[1].str());
  double cg = std::stod(cropMatch[2].str());
  double cb = std::stod(cropMatch[3].str());

  ASSERT_GE(cr, mn - 1e-6);
  ASSERT_LE(cr, mx + 1e-6);
  ASSERT_GE(cg, mn - 1e-6);
  ASSERT_LE(cg, mx + 1e-6);
  ASSERT_GE(cb, mn - 1e-6);
  ASSERT_LE(cb, mx + 1e-6);

#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}
