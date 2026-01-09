#include <gtest/gtest.h>
#include <tinyexr.h>

#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

using namespace engine;

TEST(LightmapEXRCPU, EXRToVTEX_WriteOnly)
{
  std::filesystem::create_directories("assets/lightmaps");
  const std::string exrPath  = "assets/lightmaps/cpu_tool_test.exr";
  const std::string vtexPath = "assets/lightmaps/cpu_tool_test.vtex";

  // Create small EXR
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  pixels[0] = 0.1f;
  pixels[1] = 0.2f;
  pixels[2] = 0.3f;
  pixels[3] = 1.0f;

  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  ASSERT_EQ(ret, 0);

  // Device is required by API but this test only writes CPU-side VTEX and does not load into GPU
  Window window(1, 1, "exrcpu");
  Device device(window);

  ASSERT_NO_THROW(Texture::createFromEXR_CPUOnly(device, exrPath, vtexPath, false));

  // Validate header without invoking GPU loader
  ibl_detail::vtex::Header h{};
  ASSERT_TRUE(ibl_detail::vtex::readHeader(vtexPath, h));
  EXPECT_EQ(h.width, 2u);
  EXPECT_EQ(h.height, 2u);
}

TEST(LightmapEXRCPU, EXRToVTEX_LoadIntoGPU)
{
  // Skip unless hardware tests explicitly enabled
  if (std::getenv("RUN_HARDWARE_TESTS") == nullptr)
  {
    GTEST_SKIP() << "Skipping GPU integration test (set RUN_HARDWARE_TESTS=1 to enable)";
  }

  std::filesystem::create_directories("assets/lightmaps");
  const std::string exrPath  = "assets/lightmaps/cpu_tool_test_load.exr";
  const std::string vtexPath = "assets/lightmaps/cpu_tool_test_load.vtex";

  // Create small EXR
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  pixels[0] = 0.11f;
  pixels[1] = 0.22f;
  pixels[2] = 0.33f;
  pixels[3] = 1.0f;

  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  ASSERT_EQ(ret, 0);

  Window window(1, 1, "exrcpu_load");
  Device device(window);

  // Write VTEX and load into GPU
  std::shared_ptr<Texture> tex;
  ASSERT_NO_THROW(tex = Texture::createFromEXR_CPUOnly(device, exrPath, vtexPath, true));
  ASSERT_NE(tex, nullptr);
  EXPECT_EQ(static_cast<int>(tex->getWidth()), 2);
  EXPECT_EQ(static_cast<int>(tex->getHeight()), 2);
}
