#pragma once

#include <algorithm>

#include "Engine/Tools/BakeTexel.hpp"

namespace engine::lightmap {

  // Generate a single mip level from src (width x height) to dst ((width/2) x (height/2)).
  // For each 2x2 block, average only valid texels. If none are valid, dst texel is marked invalid.
  static inline void generateMipLevel(const BakeTexel* src, int srcW, int srcH, BakeTexel* dst)
  {
    // Defensive guards to avoid UB when release-optimized
    if (src == nullptr || dst == nullptr || srcW <= 0 || srcH <= 0)
    {
      return;
    }

    int dstW = std::max(1, srcW / 2);
    int dstH = std::max(1, srcH / 2);

    auto idx = [&](int x, int y, int w) { return y * w + x; };

    for (int y = 0; y < dstH; ++y)
    {
      for (int x = 0; x < dstW; ++x)
      {
        glm::vec3 accum(0.0f);
        int       count = 0;

        for (int by = 0; by < 2; ++by)
        {
          for (int bx = 0; bx < 2; ++bx)
          {
            int sx = x * 2 + bx;
            int sy = y * 2 + by;
            if (sx >= srcW || sy >= srcH) continue;
            const BakeTexel& s = src[idx(sx, sy, srcW)];
            if (s.valid)
            {
              accum += s.radiance;
              ++count;
            }
          }
        }

        BakeTexel& d = dst[idx(x, y, dstW)];
        if (count > 0)
        {
          d.radiance = accum / float(count);
          d.valid    = 1;
        }
        else
        {
          d.radiance = glm::vec3(0.0f);
          d.valid    = 0;
        }
      }
    }
  }

} // namespace engine::lightmap
