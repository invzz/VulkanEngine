#include <algorithm>
#include <cmath>
#include <filesystem>

#include "ModelLightBaker.hpp"

// STB write is provided by EngineImporters compilation unit that defines
// STB_IMAGE_WRITE_IMPLEMENTATION, but include the header here for declarations
#include <stb_image_write.h>

namespace ModelLightBaker {

  bool ModelLightBaker::savePreviewPNG(const std::string& outDir, const std::string& baseName) const
  {
    if (hdrPixels_.empty() || imageWidth_ == 0 || imageHeight_ == 0)
    {
      return false;
    }

    int maxSize = previewMaxSize_ > 0 ? previewMaxSize_ : 512;
    int outW    = std::min(imageWidth_, maxSize);
    int outH    = std::min(imageHeight_, maxSize);
    if (outW <= 0 || outH <= 0) return false;

    std::vector<unsigned char> ldr(static_cast<size_t>(outW) * outH * 3, 0);

    // downsample by averaging blocks
    float scaleX = static_cast<float>(imageWidth_) / static_cast<float>(outW);
    float scaleY = static_cast<float>(imageHeight_) / static_cast<float>(outH);

    for (int oy = 0; oy < outH; ++oy)
    {
      for (int ox = 0; ox < outW; ++ox)
      {
        int sx0 = static_cast<int>(std::floor(ox * scaleX));
        int sy0 = static_cast<int>(std::floor(oy * scaleY));
        int sx1 = static_cast<int>(std::min(static_cast<float>(imageWidth_), std::ceil((ox + 1) * scaleX)));
        int sy1 = static_cast<int>(std::min(static_cast<float>(imageHeight_), std::ceil((oy + 1) * scaleY)));
        if (sx1 <= sx0) sx1 = sx0 + 1;
        if (sy1 <= sy0) sy1 = sy0 + 1;

        double r = 0.0, g = 0.0, b = 0.0;
        size_t count = 0;
        for (int y = sy0; y < sy1; ++y)
        {
          for (int x = sx0; x < sx1; ++x)
          {
            size_t idx = (static_cast<size_t>(y) * imageWidth_ + static_cast<size_t>(x)) * 3;
            r += hdrPixels_[idx + 0];
            g += hdrPixels_[idx + 1];
            b += hdrPixels_[idx + 2];
            ++count;
          }
        }
        if (count == 0) count = 1;
        r /= static_cast<double>(count);
        g /= static_cast<double>(count);
        b /= static_cast<double>(count);

        // simple tonemap: x -> x/(1+x), gamma 2.2
        auto tm = [](double v) {
          double t = v / (1.0 + v);
          t        = std::pow(std::max(0.0, std::min(1.0, t)), 1.0 / 2.2);
          return static_cast<unsigned char>(std::round(t * 255.0));
        };

        size_t outIdx   = (static_cast<size_t>(oy) * outW + static_cast<size_t>(ox)) * 3;
        ldr[outIdx + 0] = tm(r);
        ldr[outIdx + 1] = tm(g);
        ldr[outIdx + 2] = tm(b);
      }
    }

    std::filesystem::path p      = std::filesystem::path(outDir) / (baseName + std::string("_preview.png"));
    int                   stride = outW * 3;
    int                   ret    = stbi_write_png(p.string().c_str(), outW, outH, 3, ldr.data(), stride);
    return ret != 0;
  }

} // namespace ModelLightBaker
