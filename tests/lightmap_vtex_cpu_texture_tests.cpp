#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

using namespace engine;

TEST(LightmapVTEXCPUTexture, ReadHeaderAsCPUOnlyTexture)
{
  std::filesystem::create_directories("assets/lightmaps/ibltest");
  const std::string path = "assets/lightmaps/ibltest/prefilter_cpu.vtex";

  // Write a synthetic vtex
  ibl_detail::vtex::Header writeHeader{};
  writeHeader.vkFormat   = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
  writeHeader.width      = 8;
  writeHeader.height     = 8;
  writeHeader.mipLevels  = 3;
  writeHeader.layers     = 6;
  writeHeader.bytesPerPx = ibl_detail::vtex::bytesPerPixelFor(VK_FORMAT_R16G16B16A16_SFLOAT);

  size_t total = 0;
  for (uint32_t m = 0; m < writeHeader.mipLevels; ++m)
  {
    uint32_t const w = (std::max)(1u, writeHeader.width >> m);
    uint32_t const h = (std::max)(1u, writeHeader.height >> m);
    total += static_cast<size_t>(w) * static_cast<size_t>(h) * writeHeader.bytesPerPx * writeHeader.layers;
  }
  std::vector<char> payload(total, 0);
  ASSERT_TRUE(
          ibl_detail::vtex::writeImageFromRaw(path, payload.data(), payload.size(), VK_FORMAT_R16G16B16A16_SFLOAT, writeHeader.width, writeHeader.height, writeHeader.mipLevels, writeHeader.layers));

  // Create a cpu-only Texture instance from the vtex header (no GPU resources)
  Window window(1, 1, "vtcpu");
  Device device(window);

  auto tex = Texture::createFromVTEX(device, path, true);
  ASSERT_NE(tex, nullptr);
  EXPECT_EQ(tex->getWidth(), static_cast<int>(writeHeader.width));
  EXPECT_EQ(tex->getHeight(), static_cast<int>(writeHeader.height));
  EXPECT_EQ(tex->getMipLevels(), writeHeader.mipLevels);
}
