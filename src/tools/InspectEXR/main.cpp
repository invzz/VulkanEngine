#include <tinyexr.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "Usage: InspectEXR <file.exr> [crop=5]\n";
    return 1;
  }
  const char* filename = argv[1];
  int         crop     = 5;
  if (argc >= 3) crop = std::atoi(argv[2]);

  const char* err = nullptr;
  float*      out;
  int         width;
  int         height;
  int         ret = LoadEXR(&out, &width, &height, filename, &err);
  if (ret != TINYEXR_SUCCESS)
  {
    if (err)
    {
      std::cerr << "LoadEXR error: " << err << "\n";
      FreeEXRErrorMessage(err);
    }
    else
    {
      std::cerr << "LoadEXR failed with code " << ret << "\n";
    }
    return 2;
  }
  // tinyexr returns RGBA float pixels (r,g,b,a)
  int                channels   = 4;
  size_t             pixelCount = static_cast<size_t>(width) * height;
  std::vector<float> data(pixelCount * 3);
  for (size_t i = 0; i < pixelCount; ++i)
  {
    data[i * 3 + 0] = out[i * channels + 0];
    data[i * 3 + 1] = out[i * channels + 1];
    data[i * 3 + 2] = out[i * channels + 2];
  }
  // LoadEXR() returns a malloc()-ed float array; free it with free().
  free(out);

  float  mn  = std::numeric_limits<float>::infinity();
  float  mx  = -std::numeric_limits<float>::infinity();
  double sum = 0.0;
  for (float v : data)
  {
    mn = std::min(mn, v);
    mx = std::max(mx, v);
    sum += v;
  }
  double mean = sum / data.size();

  std::cout << "Loaded EXR: " << filename << " (" << width << "x" << height << ") channels=3" << std::endl;
  std::cout << "stats: min=" << mn << " max=" << mx << " mean=" << mean << std::endl;

  int cx = width / 2;
  int cy = height / 2;
  int r  = std::max(1, crop / 2);
  int x0 = std::max(0, cx - r);
  int x1 = std::min(width, cx + r + 1);
  int y0 = std::max(0, cy - r);
  int y1 = std::min(height, cy + r + 1);
  std::cout << "center crop (" << x0 << ":" << x1 << "," << y0 << ":" << y1 << ")" << std::endl;
  int idx = 0;
  for (int y = y0; y < y1; ++y)
  {
    for (int x = x0; x < x1; ++x)
    {
      size_t p  = static_cast<size_t>(y) * width + x;
      float  r0 = data[p * 3 + 0];
      float  g0 = data[p * 3 + 1];
      float  b0 = data[p * 3 + 2];
      std::cout << idx++ << ": [" << r0 << ", " << g0 << ", " << b0 << "]\n";
    }
  }
  return 0;
}
