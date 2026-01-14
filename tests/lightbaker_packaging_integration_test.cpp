#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

#ifdef LIGHT_BAKER_PATH
static const std::string LIGHT_BAKER_BIN = LIGHT_BAKER_PATH;
#endif

#ifdef EXR2VTEX_PATH
static const std::string EXR2VTEX_BIN = EXR2VTEX_PATH;
#endif

TEST(LightBaker, EndToEnd_EXR_To_VTEX_Packaging)
{
#ifdef LIGHT_BAKER_PATH
#ifdef MODEL_PATH
  const std::string modelPath = std::string(MODEL_PATH) + "glTF/Cube/glTF/Cube.gltf";
#else
  GTEST_SKIP() << "MODEL_PATH macro not defined";
#endif

  auto tmp = std::filesystem::temp_directory_path() / "lightbaker_pack_test_tmp";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp);

  const std::string outDir = (tmp / "out").generic_string();
  const std::string cmd    = LIGHT_BAKER_BIN + std::string(" --model ") + modelPath + " --out " + outDir + " --resolution 2";
  std::cout << "Invoking: " << cmd << "\n";

  const std::string logFile = (tmp / "lightbaker.log").generic_string();
  const std::string fullCmd = cmd + " 2>&1 | tee " + logFile;
  int               ret     = std::system(fullCmd.c_str());

  std::ifstream in(logFile);
  std::string   out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (out.find("CPU baking models is not implemented") != std::string::npos)
  {
    GTEST_SKIP() << "CPU baking not implemented in LightBaker; skipping test.";
  }

  ASSERT_EQ(ret, 0) << "LightBaker failed (exit code " << ret << ") cmd: " << cmd;

  // Find manifest (model baker writes <stem>.manifest.json)
  std::filesystem::path manifestPath = std::filesystem::path(outDir) / (std::filesystem::path(modelPath).stem().string() + std::string(".manifest.json"));
  if (!std::filesystem::exists(manifestPath))
  {
    // Fallback: look for any manifest-like file
    for (auto& p : std::filesystem::directory_iterator(outDir))
    {
      auto fname = p.path().filename().string();
      if (fname.find(".manifest.json") != std::string::npos || fname.find("_lightmap.json") != std::string::npos)
      {
        manifestPath = p.path();
        break;
      }
    }
  }

  ASSERT_TRUE(std::filesystem::exists(manifestPath)) << "Expected manifest not found in " << outDir;

  nlohmann::json j;
  {
    std::ifstream in(manifestPath);
    ASSERT_TRUE(in.good()) << "Failed to open manifest: " << manifestPath;
    in >> j;
  }

  std::string fileName = j.value("file", "");
  ASSERT_FALSE(fileName.empty()) << "Manifest missing 'file' field";

  uint32_t width  = 0;
  uint32_t height = 0;
  if (j.contains("width") && j.contains("height"))
  {
    width  = j["width"].get<uint32_t>();
    height = j["height"].get<uint32_t>();
  }
  else if (j.contains("resolution"))
  {
    auto res = j["resolution"];
    if (res.is_number_integer())
    {
      width = height = res.get<uint32_t>();
    }
    else if (res.is_array() && res.size() >= 2)
    {
      width  = res[0].get<uint32_t>();
      height = res[1].get<uint32_t>();
    }
  }

  std::filesystem::path exrPath = std::filesystem::path(outDir) / fileName;
  ASSERT_TRUE(std::filesystem::exists(exrPath)) << "Expected EXR file not found: " << exrPath;

  // Ask LightBaker to pack the produced EXR into VTEX. The pack step depends on Vulkan/hardware and
  // may fail on CPU-only CI; if packing fails and RUN_HARDWARE_TESTS is not set, skip verification.
  const std::string packCmd = LIGHT_BAKER_BIN + std::string(" --model ") + modelPath + " --out " + outDir + " --resolution 2 --pack-to-vtex";
  std::cout << "Invoking (pack): " << packCmd << "\n";

  const std::string packLog     = (tmp / "lightbaker_pack.log").generic_string();
  const std::string packFullCmd = packCmd + " 2>&1 | tee " + packLog;
  int               packRet     = std::system(packFullCmd.c_str());
  std::ifstream     pin(packLog);
  std::string       pout((std::istreambuf_iterator<char>(pin)), std::istreambuf_iterator<char>());
  if (pout.find("CPU baking models is not implemented") != std::string::npos || pout.find("CPU baker path not implemented") != std::string::npos || pout.find("CPU bake") != std::string::npos)
  {
    GTEST_SKIP() << "Pack step skipped because LightBaker reported CPU bake not implemented";
  }
  if (packRet != 0)
  {
    const char* run_hw = std::getenv("RUN_HARDWARE_TESTS");
    if (run_hw == nullptr)
    {
      GTEST_SKIP() << "Packing step skipped (no hardware/tool): LightBaker returned " << packRet;
    }
    ASSERT_EQ(packRet, 0) << "LightBaker packing failed (exit code " << packRet << ") cmd: " << packCmd;
  }

  std::filesystem::path vtexPath = std::filesystem::path(outDir) / std::filesystem::path(fileName).replace_extension(".vtex");
  if (!std::filesystem::exists(vtexPath))
  {
    const char* run_hw = std::getenv("RUN_HARDWARE_TESTS");
    if (run_hw == nullptr)
    {
      GTEST_SKIP() << "Skipping VTEX verification; VTEX not produced and RUN_HARDWARE_TESTS not set";
    }
    ASSERT_TRUE(std::filesystem::exists(vtexPath)) << "Expected VTEX file not found: " << vtexPath;
  }

  // Load the vtex and verify header
  engine::Window window(1, 1, "integ");
  engine::Device device(window);

  VkImage                          img  = VK_NULL_HANDLE;
  VkDeviceMemory                   mem  = VK_NULL_HANDLE;
  VkImageView                      view = VK_NULL_HANDLE;
  VkSampler                        samp = VK_NULL_HANDLE;
  engine::ibl_detail::vtex::Header header{};

  ASSERT_TRUE(engine::ibl_detail::vtex::loadImage(device, vtexPath.string(), img, mem, view, samp, VK_IMAGE_VIEW_TYPE_2D, 0, &header));
  if (width != 0) EXPECT_EQ(header.width, width);
  if (height != 0) EXPECT_EQ(header.height, height);

  // Clean up the Vulkan handles returned by loadImage before the device is destroyed
  engine::ibl_detail::deferDestroySampler(device, samp);
  engine::ibl_detail::deferDestroyImageView(device, view);
  engine::ibl_detail::deferFreeMemory(device, mem);
  engine::ibl_detail::deferDestroyImage(device, img);
#else
  GTEST_SKIP() << "LIGHT_BAKER_PATH macro not defined";
#endif
}
