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

TEST(MeshPushConstants, BinaryLayout)
{
  using std::byte;
  EXPECT_EQ(offsetof(MeshPushConstantData, modelMatrix), 0u);
  EXPECT_EQ(offsetof(MeshPushConstantData, normalMatrix), 64u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshId), 128u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshletBufferAddress), 136u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshletVerticesAddress), 144u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshletTrianglesAddress), 152u);
  EXPECT_EQ(offsetof(MeshPushConstantData, vertexBufferAddress), 160u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshletOffset), 168u);
  EXPECT_EQ(offsetof(MeshPushConstantData, meshletCount), 172u);
  EXPECT_EQ(offsetof(MeshPushConstantData, screenSize), 176u);
  EXPECT_EQ(offsetof(MeshPushConstantData, cullingFlags), 184u);
  EXPECT_EQ(offsetof(MeshPushConstantData, lightmapUvScale), 192u);
  EXPECT_EQ(offsetof(MeshPushConstantData, lightmapUvOffset), 200u);
  EXPECT_EQ(offsetof(MeshPushConstantData, lightmapIndex), 208u);
  EXPECT_EQ(sizeof(MeshPushConstantData), 224u);
}
