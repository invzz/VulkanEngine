#include <gtest/gtest.h>

#include "Engine/Tools/BakeTexel.hpp"
#include "Engine/Tools/dilate.hpp"

using engine::BakeTexel;
using engine::lightmap::dilateBakeTexels;

static BakeTexel make(float r, float g, float b, uint8_t v)
{
  BakeTexel t;
  t.radiance = glm::vec3(r, g, b);
  t.valid    = v;
  return t;
}

TEST(Dilate, OnePixelFill1D)
{
  int                    w = 5, h = 1;
  std::vector<BakeTexel> src(w * h, make(0, 0, 0, 0));
  src[2] = make(1.0f, 0.0f, 0.0f, 1);
  std::vector<BakeTexel> dst(w * h);

  dilateBakeTexels(src.data(), dst.data(), w, h, 1);
  // After 1 iteration, neighbors at 1 and 3 should be valid
  EXPECT_EQ(dst[1].valid, 1);
  EXPECT_EQ(dst[3].valid, 1);
  EXPECT_FLOAT_EQ(dst[1].radiance.x, 1.0f);

  dilateBakeTexels(src.data(), dst.data(), w, h, 2);
  // After 2 iterations, outer pixels should also be filled
  EXPECT_EQ(dst[0].valid, 1);
  EXPECT_EQ(dst[4].valid, 1);
}

TEST(Dilate, AverageFromMultipleNeighbors)
{
  // 3x1 where two neighbors have different radiance
  int                    w = 3, h = 1;
  std::vector<BakeTexel> src(w * h, make(0, 0, 0, 0));
  src[0] = make(1.0f, 0.0f, 0.0f, 1);
  src[2] = make(0.0f, 1.0f, 0.0f, 1);
  std::vector<BakeTexel> dst(w * h);

  dilateBakeTexels(src.data(), dst.data(), w, h, 1);
  // middle pixel should average (1,0,0) and (0,1,0) => (0.5, 0.5, 0)
  EXPECT_EQ(dst[1].valid, 1);
  EXPECT_NEAR(dst[1].radiance.x, 0.5f, 1e-6f);
  EXPECT_NEAR(dst[1].radiance.y, 0.5f, 1e-6f);
}

TEST(Dilate, 2DPropagation)
{
  int                    w = 3, h = 3;
  std::vector<BakeTexel> src(w * h, make(0, 0, 0, 0));
  // center valid
  src[1 + 1 * w] = make(2.0f, 0.0f, 0.0f, 1);
  std::vector<BakeTexel> dst(w * h);

  dilateBakeTexels(src.data(), dst.data(), w, h, 2);
  // after 2 iterations, corners should be filled
  EXPECT_EQ(dst[0].valid, 1);
  EXPECT_EQ(dst[2].valid, 1);
  EXPECT_EQ(dst[6].valid, 1);
  EXPECT_EQ(dst[8].valid, 1);
}
