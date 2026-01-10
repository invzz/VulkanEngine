#include <gtest/gtest.h>

#include "Engine/Systems/ModelRenderSystem.hpp"

using namespace engine;

TEST(MeshPushConstants, DefaultValues)
{
  MeshPushConstantData push;
  EXPECT_FLOAT_EQ(push.lightmapUvScale.x, 1.0f);
  EXPECT_FLOAT_EQ(push.lightmapUvScale.y, 1.0f);
  EXPECT_FLOAT_EQ(push.lightmapUvOffset.x, 0.0f);
  EXPECT_FLOAT_EQ(push.lightmapUvOffset.y, 0.0f);
  EXPECT_EQ(push.lightmapIndex, 0u);
}

TEST(MeshPushConstants, SetValues)
{
  MeshPushConstantData push;
  push.lightmapUvScale  = {0.25f, 0.5f};
  push.lightmapUvOffset = {0.125f, 0.0625f};
  push.lightmapIndex    = 42u;

  EXPECT_FLOAT_EQ(push.lightmapUvScale.x, 0.25f);
  EXPECT_FLOAT_EQ(push.lightmapUvScale.y, 0.5f);
  EXPECT_FLOAT_EQ(push.lightmapUvOffset.x, 0.125f);
  EXPECT_FLOAT_EQ(push.lightmapUvOffset.y, 0.0625f);
  EXPECT_EQ(push.lightmapIndex, 42u);
}
