#pragma once

#include <algorithm>
#include <vector>

#include "Engine/Tools/BakeTexel.hpp"

namespace engine::lightmap {

  // Simple seam-aware dilation: iteratively grow valid texels into neighboring invalid texels.
  // For each iteration, any invalid texel that has at least one valid 4-neighbor becomes valid
  // and receives the average radiance of its valid neighbors.
  static inline void dilateBakeTexels(const BakeTexel* src, BakeTexel* dst, int width, int height, int iterations = 1)
  {
    // Defensive guards to avoid undefined behaviour (UB) in optimized builds
    if (src == nullptr || width <= 0 || height <= 0)
    {
      return;
    }

    const int N = width * height;
    if (iterations <= 0)
    {
      // When zero iterations, behave like a no-op copy: copy src to dst (if provided and distinct)
      if (dst != nullptr && dst != src)
      {
        for (int i = 0; i < N; ++i)
          dst[i] = src[i];
      }
      return;
    }

    std::vector<BakeTexel> cur(N);
    std::vector<BakeTexel> next(N);

    // Initialize cur from src
    for (int i = 0; i < N; ++i)
      cur[i] = src[i];

    auto idx = [&](int x, int y) { return y * width + x; };

    for (int iter = 0; iter < iterations; ++iter)
    {
      // Start with copy of current state
      std::copy(cur.begin(), cur.end(), next.begin());

      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
        {
          int i = idx(x, y);
          if (cur[i].valid) continue; // already valid

          glm::vec3 accum(0.0f);
          int       count = 0;
          // 4-neighbors
          if (x > 0)
          {
            auto& n = cur[idx(x - 1, y)];
            if (n.valid)
            {
              accum += n.radiance;
              ++count;
            }
          }
          if (x < width - 1)
          {
            auto& n = cur[idx(x + 1, y)];
            if (n.valid)
            {
              accum += n.radiance;
              ++count;
            }
          }
          if (y > 0)
          {
            auto& n = cur[idx(x, y - 1)];
            if (n.valid)
            {
              accum += n.radiance;
              ++count;
            }
          }
          if (y < height - 1)
          {
            auto& n = cur[idx(x, y + 1)];
            if (n.valid)
            {
              accum += n.radiance;
              ++count;
            }
          }

          if (count > 0)
          {
            next[i].radiance = accum / (float)count;
            next[i].valid    = 1;
          }
        }
      }

      cur.swap(next);
    }

    // Write back to dst
    if (dst != nullptr)
    {
      for (int i = 0; i < N; ++i)
        dst[i] = cur[i];
    }
  }

} // namespace engine::lightmap
