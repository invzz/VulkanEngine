#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/IBLSystem.hpp"

using namespace engine;

TEST(LightmapVTEX, PrefilterHasMipsAndLayers)
{
  std::filesystem::create_directories("assets/lightmaps/ibltest_mips");

  Window window(1, 1, "VTEXMipTest");
  Device device(window);

  // Create a minimal VTEX file that simulates a prefilter with mipLevels>1 and 6 layers.
  {
    using namespace engine::ibl_detail::vtex;
    const std::filesystem::path dir = "assets/lightmaps/ibltest_mips";
    std::filesystem::create_directories(dir);
    const std::filesystem::path prefilterPath = dir / "prefilter.vtex";

    Header header{};
    header.vkFormat   = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
    header.width      = 4;
    header.height     = 4;
    header.mipLevels  = 2; // >1
    header.layers     = 6; // cubemap
    header.bytesPerPx = bytesPerPixelFor(static_cast<VkFormat>(header.vkFormat));

    size_t totalBytes = 0;
    for (uint32_t mip = 0; mip < header.mipLevels; ++mip)
    {
      uint32_t w = (std::max)(1u, header.width >> mip);
      uint32_t h = (std::max)(1u, header.height >> mip);
      totalBytes += static_cast<size_t>(w) * static_cast<size_t>(h) * header.bytesPerPx * header.layers;
    }

    std::vector<char> data(totalBytes, 0);
    std::ofstream     out(prefilterPath.generic_string(), std::ios::binary);
    ASSERT_TRUE(out.good());
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();

    // Load and validate
    VkImage        img  = VK_NULL_HANDLE;
    VkDeviceMemory mem  = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkSampler      samp = VK_NULL_HANDLE;
    Header         loaded{};

    ASSERT_TRUE(ibl_detail::vtex::loadImage(device, prefilterPath.generic_string(), img, mem, view, samp, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &loaded));

    EXPECT_GT(loaded.mipLevels, 1u);
    EXPECT_EQ(loaded.layers, 6u);
  }
}
