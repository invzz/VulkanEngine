#include <gtest/gtest.h>
#include <tinyexr.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "ModelLib/Resources/Texture.hpp"
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

TEST(LightmapEXRCPU, EXRToVTEX_BC6H_WriteOnly)
{
  // Skip unless Compressonator support was enabled at build time
#ifndef COMPRESSONATOR_CLI
  GTEST_SKIP() << "Skipping BC6H test (Compressonator support not enabled at build time).";
#endif

  std::filesystem::create_directories("assets/lightmaps");
  const std::string exrPath  = "assets/lightmaps/cpu_tool_test_bc6h.exr";
  const std::string vtexPath = "assets/lightmaps/cpu_tool_test_bc6h.vtex";

  // Create small EXR
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  pixels[0] = 0.13f;
  pixels[1] = 0.24f;
  pixels[2] = 0.35f;
  pixels[3] = 1.0f;

  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  ASSERT_EQ(ret, 0);

  Window window(1, 1, "exrcpu_bc6h");
  Device device(window);

  ASSERT_NO_THROW(Texture::createFromEXR_CPUOnly(device, exrPath, vtexPath, false, VK_FORMAT_BC6H_UFLOAT_BLOCK));

  ibl_detail::vtex::Header h{};
  ASSERT_TRUE(ibl_detail::vtex::readHeader(vtexPath, h));
  EXPECT_EQ(h.width, 2u);
  EXPECT_EQ(h.height, 2u);
  EXPECT_TRUE(h.vkFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK || h.vkFormat == VK_FORMAT_BC6H_SFLOAT_BLOCK);

  // Compressed payload should contain a DDS header produced by the Compressor; verify payload begins with 'DDS '
  std::ifstream in(vtexPath, std::ios::binary);
  ASSERT_TRUE(in);
  // Skip VTEX header
  in.seekg(static_cast<std::streamoff>(sizeof(ibl_detail::vtex::Header)), std::ios::beg);
  char magic[4] = {};
  in.read(magic, 4);
  ASSERT_EQ(in.gcount(), 4);
  EXPECT_EQ(std::string(magic, 4), std::string("DDS ")) << "Compressed payload does not start with DDS header";

  // Verify bytesPerPx indicates BC6H block size (16 bytes per block)
  EXPECT_EQ(h.bytesPerPx, 16u);
}

TEST(LightmapVTEX, WriteCompressedImageFromRaw_Roundtrip)
{
  std::filesystem::create_directories("assets/lightmaps");
  const std::string       outPath = "assets/lightmaps/compressed_roundtrip.vtex";
  const std::vector<char> payload = {'T', 'E', 'S', 'T', 0, 1, 2, 3};

  // Write compressed VTEX using a fake format (use BC6H enum to exercise compressed path semantics)
  bool ok = ibl_detail::vtex::writeCompressedImageFromRaw(outPath, payload.data(), payload.size(), VK_FORMAT_BC6H_UFLOAT_BLOCK, 4, 4, 1, 1);
  ASSERT_TRUE(ok);

  // Read back file and validate header and payload
  ibl_detail::vtex::Header h{};
  std::vector<std::byte>   data;
  // Use existing helper to read header+data
  {
    std::ifstream f(outPath, std::ios::binary);
    ASSERT_TRUE(f);
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    ASSERT_EQ(static_cast<std::streamsize>(sizeof(h)), f.gcount());
    std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes.size(), payload.size());
    EXPECT_EQ(std::memcmp(bytes.data(), payload.data(), payload.size()), 0);
  }

  EXPECT_EQ(static_cast<VkFormat>(h.vkFormat), VK_FORMAT_BC6H_UFLOAT_BLOCK);
  EXPECT_EQ(h.width, 4u);
  EXPECT_EQ(h.height, 4u);
  EXPECT_EQ(h.bytesPerPx, 16u);
}

TEST(LightmapEXRCPU, EXRToVTEX_LoadIntoGPU)
{
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
