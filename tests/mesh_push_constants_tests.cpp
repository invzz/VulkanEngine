#include <gtest/gtest.h>

#include "Engine/Systems/ModelRenderSystem.hpp"

using namespace engine;

TEST(MeshPushConstants, DefaultValues)
{
  MeshPushConstantData push;
  EXPECT_EQ(push.meshId, 0u);
  EXPECT_EQ(push.cullingFlags, 0u);
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
  EXPECT_EQ(sizeof(MeshPushConstantData), 192u);
}
