#pragma once

#include <vector>

#include "Tools/LightmapBakerLib/Geometry.hpp"

namespace LightBaker {

  struct Result
  {
    int                width  = 0;
    int                height = 0;
    std::vector<float> hdrPixels; // RGB float, row-major
  };

  // Minimal CPU-only bake that fills an HDR buffer using a simple ambient+sun approximation.
  Result bakeTriangles(const std::vector<LightmapBaker::Tri>& tris, int resolution = 0, float sunIntensity = 1.0f);

} // namespace LightBaker
