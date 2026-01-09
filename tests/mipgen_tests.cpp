#include <gtest/gtest.h>

#include "Engine/Tools/BakeTexel.hpp"
#include "Engine/Tools/mipgen.hpp"

using engine::BakeTexel;
using engine::lightmap::generateMipLevel;

static BakeTexel make(float r, float g, float b, uint8_t v)
{
  BakeTexel t;
  t.radiance = glm::vec3(r, g, b);
  t.valid    = v;
  return t;
}

TEST(MipGen, AllInvalidProducesInvalid)
{
  int                    sw = 2, sh = 2;
  std::vector<BakeTexel> src(sw * sh, make(0, 0, 0, 0));
  int                    dstW = std::max(1, sw / 2);
  int                    dstH = std::max(1, sh / 2);
  std::vector<BakeTexel> dst(dstW * dstH);

  generateMipLevel(src.data(), sw, sh, dst.data());
  EXPECT_EQ(dst[0].valid, 0);
}

TEST(MipGen, AveragesValidContributors)
{
  int                    sw = 2, sh = 2;
  std::vector<BakeTexel> src(sw * sh, make(0, 0, 0, 0));
  // top-left and bottom-right valid
  src[0]                      = make(1.0f, 0.0f, 0.0f, 1);
  src[3]                      = make(0.0f, 1.0f, 0.0f, 1);
  int                    dstW = std::max(1, sw / 2);
  int                    dstH = std::max(1, sh / 2);
  std::vector<BakeTexel> dst(dstW * dstH);

  generateMipLevel(src.data(), sw, sh, dst.data());
  EXPECT_EQ(dst[0].valid, 1);
  EXPECT_NEAR(dst[0].radiance.x, 0.5f, 1e-6f);
  EXPECT_NEAR(dst[0].radiance.y, 0.5f, 1e-6f);
}

TEST(MipGen, SingleColumn)
{
  int sw = 1, sh = 4;
  int dstW = std::max(1, sw / 2);
  int dstH = std::max(1, sh / 2);
  std::vector<BakeTexel> src(sw * sh, make(0, 0, 0, 0));
  src[0] = make(1, 0, 0, 1);
  src[2] = make(0, 1, 0, 1);

  std::vector<BakeTexel> dst(dstW * dstH);

  generateMipLevel(src.data(), sw, sh, dst.data());
  EXPECT_EQ(dst[0].valid, 1);
  EXPECT_NEAR(dst[0].radiance.x, 1.0f, 1e-6f);
  EXPECT_EQ(dst[1].valid, 1);
  EXPECT_NEAR(dst[1].radiance.y, 1.0f, 1e-6f);
}

TEST(MipGen, MipChainRespectValidity)
{
  // Build a 4x4 where only a single corner is valid; after 2 mips, result should still be valid
  int                    sw = 4, sh = 4;
  std::vector<BakeTexel> lvl0(sw * sh, make(0, 0, 0, 0));
  lvl0[0] = make(2.0f, 0.0f, 0.0f, 1);

  int                    dst1W = std::max(1, sw / 2);
  int                    dst1H = std::max(1, sh / 2);
  std::vector<BakeTexel> lvl1(dst1W * dst1H);
  generateMipLevel(lvl0.data(), sw, sh, lvl1.data());
  // lvl1 has valid at top-left
  EXPECT_EQ(lvl1[0].valid, 1);

  int                    dst2W = std::max(1, dst1W / 2);
  int                    dst2H = std::max(1, dst1H / 2);
  std::vector<BakeTexel> lvl2(dst2W * dst2H);
  generateMipLevel(lvl1.data(), dst1W, dst1H, lvl2.data());
  EXPECT_EQ(lvl2[0].valid, 1);
}
