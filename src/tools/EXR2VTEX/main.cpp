#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

using namespace engine;

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cout << "Usage: exr2vtex <input.exr> <output.vtex> [format: r32|r16]\n";
    return 1;
  }

  try
  {
    std::string inPath  = argv[1];
    std::string outPath = argv[2];
    std::string fmt     = (argc >= 4) ? argv[3] : "r32";

    Window window(16, 16, "exr2vtex");
    Device device(window);

    // Load EXR (R32 float texture)
    auto tex = engine::Texture::createFromEXR(device, inPath);
    if (!tex)
    {
      std::cerr << "Failed to load EXR: " << inPath << "\n";
      return 2;
    }

    VkFormat writeFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    if (fmt == "r16")
    {
      writeFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    // Note: writeImage expects the image to already be in the appropriate format.
    // For simplicity this tool will only write as R32 (native EXR) or fail fast when asked r16.
    if (writeFormat == VK_FORMAT_R16G16B16A16_SFLOAT)
    {
      std::cerr << "Warning: requested r16 output; using device-side blit/conversion not implemented. Falling back to r32." << std::endl;
      writeFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    // Ensure parent directory exists (be robust for user-invoked paths).
    {
      std::filesystem::path outDir(outPath);
      if (outDir.has_parent_path())
      {
        std::error_code ec;
        std::filesystem::create_directories(outDir.parent_path(), ec);
        if (ec)
        {
          std::cerr << "Failed to create output directory: " << outDir.parent_path() << "\n";
          return 4;
        }
      }
    }

    bool ok = engine::ibl_detail::vtex::writeImage(device,
                                                   outPath,
                                                   tex->getImage(),
                                                   writeFormat,
                                                   static_cast<uint32_t>(tex->getWidth()),
                                                   static_cast<uint32_t>(tex->getHeight()),
                                                   static_cast<uint32_t>(tex->getMipLevels()),
                                                   1);
    if (!ok)
    {
      std::cerr << "Failed to write VTEX: " << outPath << "\n";
      return 3;
    }

    std::cout << "Wrote VTEX: " << outPath << "\n";
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "exr2vtex: fatal: " << e.what() << "\n";
    return 10;
  }
}
