#include "Tools/LightmapBakerLib/Pack.hpp"

#include <filesystem>
#include <system_error>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "ModelLib/Resources/Texture.hpp"

namespace LightmapBaker {

  bool exrToVtex(const std::string& exrPath, const std::string& vtexPath, const std::string& fmt, std::string* err)
  {
    try
    {
      // Create a small window/device pair for Vulkan work
      engine::Window window(16, 16, "exr2vtex");
      engine::Device device(window);

      // Load EXR (R32 float texture)
      auto tex = engine::Texture::createFromEXR(device, exrPath);
      if (!tex)
      {
        if (err != nullptr) *err = std::string("Failed to load EXR: ") + exrPath;
        return false;
      }

      VkFormat writeFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
      if (fmt == "r16")
      {
        writeFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
      }

      if (writeFormat == VK_FORMAT_R16G16B16A16_SFLOAT)
      {
        // Device-side conversion not implemented: fallback to r32 and warn
        if (err != nullptr) *err = "Requested r16 output: falling back to r32 (conversion not implemented)";
        writeFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
      }

      // Ensure parent directory exists (be robust for user-invoked paths).
      {
        std::filesystem::path outDir(vtexPath);
        if (outDir.has_parent_path())
        {
          std::error_code ec;
          std::filesystem::create_directories(outDir.parent_path(), ec);
          if (ec)
          {
            if (err != nullptr) *err = std::string("Failed to create output directory: ") + outDir.parent_path().string();
            return false;
          }
        }
      }

      bool ok = engine::ibl_detail::vtex::writeImage(device,
                                                     vtexPath,
                                                     tex->getImage(),
                                                     writeFormat,
                                                     static_cast<uint32_t>(tex->getWidth()),
                                                     static_cast<uint32_t>(tex->getHeight()),
                                                     static_cast<uint32_t>(tex->getMipLevels()),
                                                     1);
      if (!ok)
      {
        if (err != nullptr) *err = std::string("Failed to write VTEX: ") + vtexPath;
        return false;
      }

      // Release the texture and flush deferred destructions now so resources are
      // destroyed before the device/window teardown (avoids validation warnings)
      tex.reset();
      vkDeviceWaitIdle(device.device());
      device.flushAllDeferred();

      return true;
    }
    catch (const std::exception& e)
    {
      if (err != nullptr) *err = std::string("exr2vtex: fatal: ") + e.what();
      return false;
    }
  }

} // namespace LightmapBaker
