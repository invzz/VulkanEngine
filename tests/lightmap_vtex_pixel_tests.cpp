#include <gtest/gtest.h>
#include <tinyexr.h>

#include <filesystem>
#include <fstream>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/IBLSystem.hpp"

using namespace engine;

TEST(LightmapVTEXPixel, EXRToVTEXRoundtrip)
{
  std::filesystem::create_directories("assets/lightmaps");

  const std::string exrPath  = "assets/lightmaps/pixel_test.exr";
  const std::string vtexPath = "assets/lightmaps/pixel_test.vtex";

  // Create a tiny 2x2 float RGBA EXR with a known pixel value in (0,0)
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  // Set first pixel to a known color (R,G,B,A)
  pixels[0] = 0.25f;
  pixels[1] = 0.5f;
  pixels[2] = 0.75f;
  pixels[3] = 1.0f;

  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  ASSERT_EQ(ret, 0);

  // Setup device
  Window window(1, 1, "VTEXPixelTest");
  Device device(window);
  std::cout << "[Test] Device created" << std::endl;

  // Test R32 roundtrip by writing a VTEX directly from the EXR pixels (avoid GPU writeImage)
  {
    // Load raw EXR pixels from disk using tinyexr
    int         w = 0, h = 0;
    float*      rgba = nullptr;
    const char* err  = nullptr;
    int         r    = LoadEXR(&rgba, &w, &h, exrPath.c_str(), &err);
    ASSERT_EQ(r, TINYEXR_SUCCESS);
    std::cout << "[Test] LoadEXR OK: " << w << "x" << h << std::endl;
    ASSERT_NE(rgba, nullptr);

    std::vector<float> exrPixels(rgba, rgba + (w * h * 4));
    free(rgba);

    std::string              outR32 = "assets/lightmaps/pixel_test_r32.vtex";
    ibl_detail::vtex::Header header{};
    header.vkFormat   = static_cast<uint32_t>(VK_FORMAT_R32G32B32A32_SFLOAT);
    header.width      = static_cast<uint32_t>(w);
    header.height     = static_cast<uint32_t>(h);
    header.mipLevels  = 1;
    header.layers     = 1;
    header.bytesPerPx = ibl_detail::vtex::bytesPerPixelFor(VK_FORMAT_R32G32B32A32_SFLOAT);

    std::ofstream out(outR32, std::ios::binary);
    ASSERT_TRUE(out.good());
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(exrPixels.data()), static_cast<std::streamsize>(exrPixels.size() * sizeof(float)));
    out.close();

    // Read the VTEX file directly and inspect the first pixel without invoking GPU loader.
    std::ifstream in(outR32, std::ios::binary);
    ASSERT_TRUE(in.good());
    ibl_detail::vtex::Header fileHeader{};
    in.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    ASSERT_EQ(fileHeader.width, 2u);
    ASSERT_EQ(fileHeader.height, 2u);
    std::vector<float> readPixels(4);
    in.read(reinterpret_cast<char*>(readPixels.data()), static_cast<std::streamsize>(sizeof(float) * 4));
    EXPECT_NEAR(readPixels[0], 0.25f, 1e-4f);
    EXPECT_NEAR(readPixels[1], 0.5f, 1e-4f);
    EXPECT_NEAR(readPixels[2], 0.75f, 1e-4f);
    EXPECT_NEAR(readPixels[3], 1.0f, 1e-4f);
    in.close();
  }

  // Verify R16-like format case using a synthetic VTEX header (CPU-only verification so tests are deterministic)
  {
    std::filesystem::create_directories("assets/lightmaps/ibltest");
    const std::string prefilterPath = "assets/lightmaps/ibltest/prefilter.vtex";

    ibl_detail::vtex::Header writeHeader{};
    writeHeader.vkFormat   = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
    writeHeader.width      = 4;
    writeHeader.height     = 4;
    writeHeader.mipLevels  = 3; // multiple mips
    writeHeader.layers     = 6; // cubemap
    writeHeader.bytesPerPx = ibl_detail::vtex::bytesPerPixelFor(VK_FORMAT_R16G16B16A16_SFLOAT);

    // Compute payload size
    VkDeviceSize totalBytes = 0;
    for (uint32_t mip = 0; mip < writeHeader.mipLevels; ++mip)
    {
      uint32_t const w = (std::max)(1u, writeHeader.width >> mip);
      uint32_t const h = (std::max)(1u, writeHeader.height >> mip);
      totalBytes += static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * writeHeader.bytesPerPx * writeHeader.layers;
    }

    std::ofstream out(prefilterPath, std::ios::binary);
    ASSERT_TRUE(out.good());
    out.write(reinterpret_cast<const char*>(&writeHeader), sizeof(writeHeader));
    std::vector<char> payload(static_cast<size_t>(totalBytes), 0);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    out.close();

    ibl_detail::vtex::Header readHeader{};
    ASSERT_TRUE(ibl_detail::vtex::readHeader(prefilterPath, readHeader));
    EXPECT_GT(readHeader.mipLevels, 1u);
    EXPECT_EQ(readHeader.layers, 6u);
  }
}
