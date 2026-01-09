#include <gtest/gtest.h>
#include <tinyexr.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

using namespace engine;

TEST(LightmapEXR2VTEX, ToolConvertsEXRToVTEX)
{
  std::filesystem::create_directories("assets/lightmaps");
  const std::string exrPath  = "assets/lightmaps/tool_test.exr";
  const std::string vtexPath = "assets/lightmaps/tool_test.vtex";

  // Create small EXR
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  pixels[0]       = 0.3f;
  pixels[1]       = 0.4f;
  pixels[2]       = 0.5f;
  pixels[3]       = 1.0f;
  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  ASSERT_EQ(ret, 0);

  // Skip hardware-dependent test unless explicitly enabled via RUN_HARDWARE_TESTS or the EXR2VTEX_PATH exists.
  {
    const char* run_hw      = std::getenv("RUN_HARDWARE_TESTS");
    const char* env_path    = std::getenv("EXR2VTEX_PATH");
    bool        tool_exists = false;
    if (env_path && std::filesystem::exists(env_path))
    {
      tool_exists = true;
    }
    else if (std::filesystem::exists(EXR2VTEX_PATH))
    {
      tool_exists = true;
    }

    if (run_hw == nullptr && !tool_exists)
    {
      GTEST_SKIP() << "Skipping hardware-dependent test (set RUN_HARDWARE_TESTS=1 or provide EXR2VTEX_PATH)";
    }
  }

  // Call the built tool to convert. Prefer the project-level tools/ directory, then fall back to common build locations.
#
  std::vector<std::string> candidates = {
          EXR2VTEX_PATH,
  };

  int  rc      = -1;
  bool invoked = false;
  for (auto& p : candidates)
  {
    if (std::filesystem::exists(p))
    {
      std::string cmd = p + " " + exrPath + " " + vtexPath;
      rc              = std::system(cmd.c_str());
      invoked         = true;
      break;
    }
  }
  ASSERT_TRUE(invoked) << "EXR2VTEX executable not found in candidates";
  ASSERT_EQ(rc, 0);
  // Load the vtex and verify header
  Window window(1, 1, "integ");
  Device device(window);

  VkImage                  img  = VK_NULL_HANDLE;
  VkDeviceMemory           mem  = VK_NULL_HANDLE;
  VkImageView              view = VK_NULL_HANDLE;
  VkSampler                samp = VK_NULL_HANDLE;
  ibl_detail::vtex::Header header{};

  ASSERT_TRUE(ibl_detail::vtex::loadImage(device, vtexPath, img, mem, view, samp, VK_IMAGE_VIEW_TYPE_2D, 0, &header));
  EXPECT_EQ(header.width, 2u);
  EXPECT_EQ(header.height, 2u);
}
