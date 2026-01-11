#include "Tools/LightBaker/LightBaker.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <iostream>

using namespace LightBaker;
using namespace LightmapBaker;

Result LightBaker::bakeTriangles(const std::vector<Tri>& tris, int resolution, float sunIntensity)
{
  Result res;
  if (tris.empty())
  {
    std::cerr << "[LightBaker] No triangles provided to bakeTriangles\n";
    return res;
  }

  // Compute bounds
  engine::AABB sceneBounds;
  for (const auto& t : tris)
  {
    sceneBounds.expand(t.p0);
    sceneBounds.expand(t.p1);
    sceneBounds.expand(t.p2);
  }
  float sceneExtent = glm::length(sceneBounds.max - sceneBounds.min);

  // Choose resolution heuristically if not supplied
  if (resolution <= 0)
  {
    resolution = sceneExtent > 5.0f ? 512 : 256;
  }

  res.width  = resolution;
  res.height = resolution;
  res.hdrPixels.assign(static_cast<size_t>(res.width) * static_cast<size_t>(res.height) * 3, 0.0f);

  const float ambient = 0.05f;
  // Simple placeholder: constant ambient + sun intensity everywhere
  for (size_t i = 0; i < static_cast<size_t>(res.width) * static_cast<size_t>(res.height); ++i)
  {
    float val                  = ambient + sunIntensity;
    res.hdrPixels[(i * 3) + 0] = val;
    res.hdrPixels[(i * 3) + 1] = val;
    res.hdrPixels[(i * 3) + 2] = val;
  }

  std::cout << "[LightBaker] bakeTriangles produced " << res.width << "x" << res.height << " bake (placeholder)\n";
  return res;
}
