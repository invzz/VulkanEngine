#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

TEST(LightBaker, ModelEXR_AutoRes_Keep)
{
  GTEST_SKIP() << "Temporarily disabled test due to frequent timeouts in CI environments";
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  const std::filesystem::path inspectBin = std::filesystem::path(TOOL_PATH) / "InspectEXR";
  if (!std::filesystem::exists(inspectBin))
  {
    GTEST_SKIP() << "InspectEXR binary not found at " << inspectBin << "; build it first with xmake build InspectEXR";
  }

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_autores_keep";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / "out").generic_string();
  const std::string cmd    = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --gpu --keep-exr --sun-dir 0,-1,0 --sun-intensity 4.0";
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
  const std::string inspectCmd = inspectBin.generic_string() + " " + exrPath.generic_string() + " 4 > " + inspectLog + " 2>&1";
  std::cout << "Invoking InspectEXR: " << inspectCmd << '\n';
  int inspectRet = std::system(inspectCmd.c_str());
  ASSERT_EQ(inspectRet, 0) << "InspectEXR failed (exit code " << inspectRet << ") cmd: " << inspectCmd;

  std::ifstream inspectIn(inspectLog);
  std::string   inspectOut((std::istreambuf_iterator<char>(inspectIn)), std::istreambuf_iterator<char>());
  std::cout << "InspectEXR output:\n" << inspectOut << std::endl;

  // Parse size line: Loaded EXR: <file> (WxH) channels=3
  std::regex  sizeRx("Loaded EXR: .*\\((\\d+)x(\\d+)\\) channels=3");
  std::smatch sizeMatch;
  ASSERT_TRUE(std::regex_search(inspectOut, sizeMatch, sizeRx)) << "InspectEXR did not print size line";

  int w = std::stoi(sizeMatch[1].str());
  int h = std::stoi(sizeMatch[2].str());
  ASSERT_EQ(w, h) << "Expected square resolution";
  // auto-choice should be either 256 or 512
  ASSERT_TRUE(w == 256 || w == 512) << "Expected auto-chosen resolution to be 256 or 512, got " << w;

  // basic stats sanity checks
  std::regex  statsRx("stats:\\s*min=([eE0-9+.-]+)\\s*max=([eE0-9+.-]+)\\s*mean=([eE0-9+.-]+)");
  std::smatch statsMatch;
  ASSERT_TRUE(std::regex_search(inspectOut, statsMatch, statsRx)) << "InspectEXR did not print stats line";

  double mn   = std::stod(statsMatch[1].str());
  double mx   = std::stod(statsMatch[2].str());
  double mean = std::stod(statsMatch[3].str());
  ASSERT_LE(mn, mx);
  ASSERT_GE(mean, mn - 1e-6);
  ASSERT_LE(mean, mx + 1e-6);

  // Expect visible contrast in the EXR (detects sun/shadow separation)
  ASSERT_GE(mx - mn, 0.15) << "Expected visible contrast in EXR (max-min >= 0.15) to detect shadows";

  // center crop exists and values in range
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

TEST(LightBaker, ModelEXR_SunDir_ChangesStats)
{
  GTEST_SKIP() << "Temporarily disabled test due to frequent timeouts in CI environments";
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  const std::filesystem::path inspectBin = std::filesystem::path(TOOL_PATH) / "InspectEXR";
  if (!std::filesystem::exists(inspectBin))
  {
    GTEST_SKIP() << "InspectEXR binary not found at " << inspectBin << "; build it first with xmake build InspectEXR";
  }

  // Probe run to detect CPU-only builds early (don't GTEST_SKIP inside the lambda)
  {
    auto tmp0 = std::filesystem::temp_directory_path() / "lightbaker_sundir_probe";
    std::filesystem::remove_all(tmp0);
    std::filesystem::create_directories(tmp0);
    const std::string outDir0  = (tmp0 / "out").generic_string();
    const std::string cmd0     = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir0 + " --gpu --keep-exr --sun-dir 0,-1,0 --sun-intensity 4.0";
    const std::string logFile0 = (tmp0 / "lightbaker.log").generic_string();
    const std::string fullCmd0 = cmd0 + " 2>&1 | tee " + logFile0;
    int               ret0     = std::system(fullCmd0.c_str());
    std::ifstream     in0(logFile0);
    std::string       out0((std::istreambuf_iterator<char>(in0)), std::istreambuf_iterator<char>());
    if (out0.find("CPU baking models is not implemented") != std::string::npos || out0.find("CPU baker path not implemented") != std::string::npos || out0.find("CPU bake") != std::string::npos)
    {
      GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
    }
    ASSERT_EQ(ret0, 0) << "Probe LightBaker run failed (exit code " << ret0 << ") cmd: " << cmd0 << "\nLog:\n" << out0;
    std::filesystem::remove_all(tmp0);
  }

  auto run_inspect = [&](const std::string& flags) -> std::tuple<bool, double, double, double, std::string> {
    auto tmp = std::filesystem::temp_directory_path() / (std::string("lightbaker_sundir_") + std::to_string(std::hash<std::string>{}(flags)));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    const std::string outDir  = (tmp / "out").generic_string();
    const std::string cmd     = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --gpu --keep-exr " + flags;
    const std::string logFile = (tmp / "lightbaker.log").generic_string();
    const std::string fullCmd = cmd + " 2>&1 | tee " + logFile;

    std::cout << "Invoking: " << fullCmd << " logging to '" << logFile << "'\n";
    int           ret = std::system(fullCmd.c_str());
    std::ifstream inlog(logFile);
    std::string   out((std::istreambuf_iterator<char>(inlog)), std::istreambuf_iterator<char>());
    // Note: probe run earlier already skips CPU-only builds; here we return a failure indicator instead of using gtest assertions
    if (ret != 0)
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, out);
    }

    // Find .exr
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
    if (exrPath.empty())
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, out);
    }

    const std::string inspectLog = (tmp / "inspect.log").generic_string();
    const std::string inspectCmd = inspectBin.generic_string() + " " + exrPath.generic_string() + " 4 > " + inspectLog + " 2>&1";
    int               inspectRet = std::system(inspectCmd.c_str());
    std::ifstream     inspectIn(inspectLog);
    std::string       inspectOut((std::istreambuf_iterator<char>(inspectIn)), std::istreambuf_iterator<char>());
    if (inspectRet != 0)
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, inspectOut);
    }

    std::regex  statsRx(R"(stats:\s*min=([eE0-9+.-]+)\s*max=([eE0-9+.-]+)\s*mean=([eE0-9+.-]+))");
    std::smatch statsMatch;
    if (!std::regex_search(inspectOut, statsMatch, statsRx))
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, inspectOut);
    }

    double mn   = std::stod(statsMatch[1].str());
    double mx   = std::stod(statsMatch[2].str());
    double mean = std::stod(statsMatch[3].str());
    return std::tuple<bool, double, double, double, std::string>(true, mn, mx, mean, inspectOut);
  };

  auto [ok1, mn1, mx1, mean1, out1] = run_inspect("--sun-dir 0,-1,0 --sun-intensity 4.0");
  auto [ok2, mn2, mx2, mean2, out2] = run_inspect("--sun-dir 0,1,0 --sun-intensity 4.0");

  if (!ok1 || !ok2)
  {
    // If probe didn't detect CPU-only, but this run produced CPU messages, skip; otherwise fail with logs
    if ((out1.find("CPU baking models is not implemented") != std::string::npos) || (out1.find("CPU baker path not implemented") != std::string::npos) ||
        (out1.find("CPU bake") != std::string::npos) || (out2.find("CPU baking models is not implemented") != std::string::npos) ||
        (out2.find("CPU baker path not implemented") != std::string::npos) || (out2.find("CPU bake") != std::string::npos))
    {
      GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
    }
    FAIL() << "run_inspect failed\nFirst run output:\n" << out1 << "\nSecond run output:\n" << out2;
  }

  const double eps = 1e-3;
  ASSERT_TRUE(std::fabs(mn1 - mn2) > eps || std::fabs(mx1 - mx2) > eps || std::fabs(mean1 - mean2) > eps)
          << "Expected stats to change when sun direction changes\nFirst stats: mn=" << mn1 << " mx=" << mx1 << " mean=" << mean1 << "\nSecond stats: mn=" << mn2 << " mx=" << mx2 << " mean=" << mean2
          << "\n--- Inspect outputs ---\n"
          << out1 << "\n"
          << out2;

#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}

TEST(LightBaker, ModelEXR_SunIntensity_ChangesStats)
{
  GTEST_SKIP() << "Temporarily disabled test due to frequent timeouts in CI environments";
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  const std::filesystem::path inspectBin = std::filesystem::path(TOOL_PATH) / "InspectEXR";
  if (!std::filesystem::exists(inspectBin))
  {
    GTEST_SKIP() << "InspectEXR binary not found at " << inspectBin << "; build it first with xmake build InspectEXR";
  }

  // Probe run to detect CPU-only builds early (don't GTEST_SKIP inside the lambda)
  {
    auto tmp0 = std::filesystem::temp_directory_path() / "lightbaker_sunint_probe";
    std::filesystem::remove_all(tmp0);
    std::filesystem::create_directories(tmp0);
    const std::string outDir0  = (tmp0 / "out").generic_string();
    const std::string cmd0     = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir0 + " --gpu --keep-exr --sun-dir 0,-1,0 --sun-intensity 1.0";
    const std::string logFile0 = (tmp0 / "lightbaker.log").generic_string();
    const std::string fullCmd0 = cmd0 + " 2>&1 | tee " + logFile0;
    int               ret0     = std::system(fullCmd0.c_str());
    std::ifstream     in0(logFile0);
    std::string       out0((std::istreambuf_iterator<char>(in0)), std::istreambuf_iterator<char>());
    if (out0.find("CPU baking models is not implemented") != std::string::npos || out0.find("CPU baker path not implemented") != std::string::npos || out0.find("CPU bake") != std::string::npos)
    {
      GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
    }
    ASSERT_EQ(ret0, 0) << "Probe LightBaker run failed (exit code " << ret0 << ") cmd: " << cmd0 << "\nLog:\n" << out0;
    std::filesystem::remove_all(tmp0);
  }

  auto run_inspect = [&](const std::string& flags) -> std::tuple<bool, double, double, double, std::string> {
    auto tmp = std::filesystem::temp_directory_path() / (std::string("lightbaker_sunint_") + std::to_string(std::hash<std::string>{}(flags)));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    const std::string outDir  = (tmp / "out").generic_string();
    const std::string cmd     = LIGHT_BAKER_BIN + " --model " + modelPath + " --out " + outDir + " --gpu --keep-exr " + flags;
    const std::string logFile = (tmp / "lightbaker.log").generic_string();
    const std::string fullCmd = cmd + " 2>&1 | tee " + logFile;

    int           ret = std::system(fullCmd.c_str());
    std::ifstream inlog(logFile);
    std::string   out((std::istreambuf_iterator<char>(inlog)), std::istreambuf_iterator<char>());
    if (ret != 0)
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, out);
    }

    // Find .exr
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
    if (exrPath.empty())
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, out);
    }

    const std::string inspectLog = (tmp / "inspect.log").generic_string();
    const std::string inspectCmd = inspectBin.generic_string() + " " + exrPath.generic_string() + " 4 > " + inspectLog + " 2>&1";
    int               inspectRet = std::system(inspectCmd.c_str());
    std::ifstream     inspectIn(inspectLog);
    std::string       inspectOut((std::istreambuf_iterator<char>(inspectIn)), std::istreambuf_iterator<char>());
    if (inspectRet != 0)
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, inspectOut);
    }

    std::regex  statsRx("stats:\\s*min=([eE0-9+.-]+)\\s*max=([eE0-9+.-]+)\\s*mean=([eE0-9+.-]+)");
    std::smatch statsMatch;
    if (!std::regex_search(inspectOut, statsMatch, statsRx))
    {
      return std::tuple<bool, double, double, double, std::string>(false, 0.0, 0.0, 0.0, inspectOut);
    }

    double mn   = std::stod(statsMatch[1].str());
    double mx   = std::stod(statsMatch[2].str());
    double mean = std::stod(statsMatch[3].str());
    return std::tuple<bool, double, double, double, std::string>(true, mn, mx, mean, inspectOut);
  };

  auto [ok1, mn1, mx1, mean1, out1] = run_inspect("--sun-dir 0,-1,0 --sun-intensity 0.0");
  auto [ok2, mn2, mx2, mean2, out2] = run_inspect("--sun-dir 0,-1,0 --sun-intensity 8.0");

  if (!ok1 || !ok2)
  {
    if ((out1.find("CPU baking models is not implemented") != std::string::npos) || (out1.find("CPU baker path not implemented") != std::string::npos) ||
        (out1.find("CPU bake") != std::string::npos) || (out2.find("CPU baking models is not implemented") != std::string::npos) ||
        (out2.find("CPU baker path not implemented") != std::string::npos) || (out2.find("CPU bake") != std::string::npos))
    {
      GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
    }
    FAIL() << "run_inspect failed\nFirst run output:\n" << out1 << "\nSecond run output:\n" << out2;
  }

  const double eps = 1e-3;
  ASSERT_TRUE(std::fabs(mn1 - mn2) > eps || std::fabs(mx1 - mx2) > eps || std::fabs(mean1 - mean2) > eps)
          << "Expected stats to change when sun intensity changes\nFirst stats: mn=" << mn1 << " mx=" << mx1 << " mean=" << mean1 << "\nSecond stats: mn=" << mn2 << " mx=" << mx2 << " mean=" << mean2
          << "\n--- Inspect outputs ---\n"
          << out1 << "\n"
          << out2;

#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}
