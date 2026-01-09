#include <cstdio>

#include "Engine/Tools/BakeTexel.hpp"
#include "Engine/Tools/mipgen.hpp"

int main()
{
  int                            sw = 1, sh = 4;
  int                            dstW = std::max(1, sw / 2);
  int                            dstH = std::max(1, sh / 2);
  std::vector<engine::BakeTexel> src(sw * sh);
  src[0].radiance = glm::vec3(1, 0, 0);
  src[0].valid    = 1;
  src[2].radiance = glm::vec3(0, 1, 0);
  src[2].valid    = 1;
  std::vector<engine::BakeTexel> dst(dstW * dstH);
  printf("Before generate: dstW=%d dstH=%d dst.size=%zu\n", dstW, dstH, dst.size());
  engine::lightmap::generateMipLevel(src.data(), sw, sh, dst.data());
  printf("After generate: dst[0].valid=%d\n", (int)dst[0].valid);
  return 0;
}
