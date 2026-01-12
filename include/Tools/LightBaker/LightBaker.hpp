#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
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

  // Compute a canonical string key for a model node primitive suitable for hashing/deduplication
  // uvChannel / uvScale / uvOffset are included to ensure different UV layouts map to different bakes.
  inline std::string makeCanonicalPrimitiveKey(const engine::Model::Builder& builder,
                                               const std::string&            modelPath,
                                               int                           nodeIndex,
                                               int                           primitiveIndex,
                                               int                           resolution,
                                               int                           uvChannel = 1,
                                               double                        uvScaleX  = 1.0,
                                               double                        uvScaleY  = 1.0,
                                               double                        uvOffsetX = 0.0,
                                               double                        uvOffsetY = 0.0)
  {
    std::ostringstream key;
    key << "model=" << modelPath << "|node=" << nodeIndex << "|prim=" << primitiveIndex << "|res=" << resolution;

    // Quantize UV params deterministically to avoid floating point variability
    auto qUvScaleX  = std::llround(uvScaleX * 1000000.0);
    auto qUvScaleY  = std::llround(uvScaleY * 1000000.0);
    auto qUvOffsetX = std::llround(uvOffsetX * 1000000.0);
    auto qUvOffsetY = std::llround(uvOffsetY * 1000000.0);

    key << "|uvc=" << uvChannel << "|uvs=" << qUvScaleX << "," << qUvScaleY << "|uvo=" << qUvOffsetX << "," << qUvOffsetY;

    std::string k = std::to_string(nodeIndex) + "_" + std::to_string(primitiveIndex);
    if (builder.primitiveVertexOffsets.contains(k))
    {
      uint32_t start = builder.primitiveVertexOffsets.at(k);
      uint32_t count = builder.primitiveVertexCounts.at(k);
      key << "|vcount=" << count;
      long long checksum = 0;
      for (uint32_t vi = start; vi < start + count; ++vi)
      {
        auto p = builder.vertices[vi].position;
        checksum += std::llround(p.x * 1000000.0);
        checksum += std::llround(p.y * 1000000.0) * 7;
        checksum += std::llround(p.z * 1000000.0) * 13;
      }
      key << "|psum=" << checksum;
    }
    return key.str();
  }

  // Create a short stable lightmap id from a canonical key string
  inline std::string makeLightmapIdFromKey(const std::string& canonicalKey)
  {
    size_t             h = std::hash<std::string>{}(canonicalKey);
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << (h & 0xffffffffffffffffull);
    std::string s = ss.str();
    return std::string("lm_") + s.substr(0, 8);
  }

} // namespace LightBaker
