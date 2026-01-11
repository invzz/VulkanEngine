#pragma once

#include <string>

namespace LightmapBaker {

  // Convert an EXR file to a VTEX file using an ephemeral Vulkan device.
  // Returns true on success. If `err` is provided, it will contain an error message on failure.
  bool exrToVtex(const std::string& exrPath, const std::string& vtexPath, const std::string& fmt = "r32", std::string* err = nullptr);

} // namespace LightmapBaker
