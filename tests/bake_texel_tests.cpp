#include <gtest/gtest.h>

#include "Engine/Tools/BakeTexel.hpp"
#include "glm/vec3.hpp"

TEST(BakeTexel, DefaultValues)
{
  engine::BakeTexel t;
  EXPECT_EQ(t.valid, 0);
  EXPECT_FLOAT_EQ(t.radiance.x, 0.0f);
  EXPECT_FLOAT_EQ(t.radiance.y, 0.0f);
  EXPECT_FLOAT_EQ(t.radiance.z, 0.0f);
}

TEST(BakeTexel, SetAndGet)
{
  engine::BakeTexel t;
  t.radiance = glm::vec3(1.0f, 0.5f, 0.25f);
  t.valid    = 1;
  EXPECT_EQ(t.valid, 1);
  EXPECT_FLOAT_EQ(t.radiance.x, 1.0f);
  EXPECT_FLOAT_EQ(t.radiance.y, 0.5f);
  EXPECT_FLOAT_EQ(t.radiance.z, 0.25f);
}
