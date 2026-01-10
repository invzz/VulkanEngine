#include <gtest/gtest.h>
#include <tinyexr.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>

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

    ASSERT_TRUE(
            ibl_detail::vtex::writeImageFromRaw(outR32, exrPixels.data(), exrPixels.size() * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1, 1));

    // Read the VTEX file header and inspect the first pixel without invoking GPU loader.
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

  // Real EXR -> VTEX (R16F) roundtrip via CPU helper (float->half conversion)
  {
    std::string outR16 = "assets/lightmaps/pixel_test_r16.vtex";
    if (std::filesystem::exists(outR16)) std::filesystem::remove(outR16);
    ASSERT_NO_THROW(Texture::createFromEXR_CPUOnly(device, exrPath, outR16, false, VK_FORMAT_R16G16B16A16_SFLOAT));

    ibl_detail::vtex::Header fileHeader{};
    ASSERT_TRUE(ibl_detail::vtex::readHeader(outR16, fileHeader));
    ASSERT_EQ(fileHeader.width, 2u);
    ASSERT_EQ(fileHeader.height, 2u);
    ASSERT_EQ(fileHeader.bytesPerPx, ibl_detail::vtex::bytesPerPixelFor(VK_FORMAT_R16G16B16A16_SFLOAT));

    // Read first pixel halfs (RGBA) from file and convert to float for comparison
    std::ifstream in16(outR16, std::ios::binary);
    ASSERT_TRUE(in16.good());
    ibl_detail::vtex::Header hdr16{};
    in16.read(reinterpret_cast<char*>(&hdr16), sizeof(hdr16));
    std::vector<uint16_t> halfs(4);
    in16.read(reinterpret_cast<char*>(halfs.data()), static_cast<std::streamsize>(sizeof(uint16_t) * halfs.size()));
    in16.close();

    auto halfToFloat = [](uint16_t h) -> float {
      uint32_t s = (h >> 15) & 0x1u;
      uint32_t e = (h >> 10) & 0x1Fu;
      uint32_t m = h & 0x3FFu;
      if (e == 0)
      {
        if (m == 0) return s ? -0.0f : 0.0f;
        // subnormal
        float mant = static_cast<float>(m) / 1024.0f;
        float val = std::ldexp(mant, -14);
        return s ? -val : val;
      }
      else if (e == 31)
      {
        // inf or NaN
        return s ? -INFINITY : INFINITY;
      }
      else
      {
        int32_t exp = static_cast<int32_t>(e) - 15 + 127;
        uint32_t mant32 = m << 13;
        uint32_t bits = (s << 31) | (static_cast<uint32_t>(exp) << 23) | mant32;
        float val;
        std::memcpy(&val, &bits, sizeof(val));
        return val;
      }
    };

    std::vector<float> readFloats(4);
    for (size_t i = 0; i < 4; ++i) readFloats[i] = halfToFloat(halfs[i]);

    EXPECT_NEAR(readFloats[0], 0.25f, 1e-3f);
    EXPECT_NEAR(readFloats[1], 0.5f, 1e-3f);
    EXPECT_NEAR(readFloats[2], 0.75f, 1e-3f);
    EXPECT_NEAR(readFloats[3], 1.0f, 1e-3f);
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
