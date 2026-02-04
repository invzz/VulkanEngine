#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

#include "ModelLib/Resources/Model.hpp"

using namespace engine;

// =============================================================================
// AABB Default Construction Tests
// =============================================================================

TEST(AABB, GivenDefaultConstruction_WhenInspected_ThenMinIsMaxFloat)
{
  AABB box;
  EXPECT_FLOAT_EQ(box.min.x, std::numeric_limits<float>::max());
  EXPECT_FLOAT_EQ(box.min.y, std::numeric_limits<float>::max());
  EXPECT_FLOAT_EQ(box.min.z, std::numeric_limits<float>::max());
}

TEST(AABB, GivenDefaultConstruction_WhenInspected_ThenMaxIsLowestFloat)
{
  AABB box;
  EXPECT_FLOAT_EQ(box.max.x, std::numeric_limits<float>::lowest());
  EXPECT_FLOAT_EQ(box.max.y, std::numeric_limits<float>::lowest());
  EXPECT_FLOAT_EQ(box.max.z, std::numeric_limits<float>::lowest());
}

TEST(AABB, GivenDefaultConstruction_WhenInspected_ThenIsNotValid)
{
  AABB box;
  EXPECT_FALSE(box.isValid());
}

// =============================================================================
// AABB Expand Tests
// =============================================================================

TEST(AABB, GivenEmptyBox_WhenExpandedWithSinglePoint_ThenBoxContainsPoint)
{
  AABB box;
  box.expand(glm::vec3(1.0f, 2.0f, 3.0f));

  EXPECT_EQ(box.min, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(box.max, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_TRUE(box.isValid());
}

TEST(AABB, GivenEmptyBox_WhenExpandedWithTwoPoints_ThenBoxContainsBothPoints)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -2.0f, -3.0f));
  box.expand(glm::vec3(1.0f, 2.0f, 3.0f));

  EXPECT_EQ(box.min, glm::vec3(-1.0f, -2.0f, -3.0f));
  EXPECT_EQ(box.max, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(AABB, GivenEmptyBox_WhenExpandedWithOrigin_ThenBoxContainsOrigin)
{
  AABB box;
  box.expand(glm::vec3(0.0f, 0.0f, 0.0f));

  EXPECT_EQ(box.min, glm::vec3(0.0f, 0.0f, 0.0f));
  EXPECT_EQ(box.max, glm::vec3(0.0f, 0.0f, 0.0f));
  EXPECT_TRUE(box.isValid());
}

TEST(AABB, GivenBox_WhenExpandedWithPointInsideBox_ThenBoxUnchanged)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::vec3 oldMin = box.min;
  glm::vec3 oldMax = box.max;

  box.expand(glm::vec3(0.0f, 0.0f, 0.0f));

  EXPECT_EQ(box.min, oldMin);
  EXPECT_EQ(box.max, oldMax);
}

TEST(AABB, GivenBox_WhenExpandedWithPointOutsideBox_ThenBoxGrows)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  box.expand(glm::vec3(5.0f, 0.0f, 0.0f));

  EXPECT_FLOAT_EQ(box.max.x, 5.0f);
  EXPECT_FLOAT_EQ(box.max.y, 1.0f);
  EXPECT_FLOAT_EQ(box.max.z, 1.0f);
}

// =============================================================================
// AABB Center and Extents Tests
// =============================================================================

TEST(AABB, GivenUnitBox_WhenCenterCalculated_ThenReturnsOrigin)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::vec3 center = box.center();

  EXPECT_FLOAT_EQ(center.x, 0.0f);
  EXPECT_FLOAT_EQ(center.y, 0.0f);
  EXPECT_FLOAT_EQ(center.z, 0.0f);
}

TEST(AABB, GivenOffsetBox_WhenCenterCalculated_ThenReturnsCorrectCenter)
{
  AABB box;
  box.expand(glm::vec3(2.0f, 4.0f, 6.0f));
  box.expand(glm::vec3(4.0f, 8.0f, 10.0f));

  glm::vec3 center = box.center();

  EXPECT_FLOAT_EQ(center.x, 3.0f);
  EXPECT_FLOAT_EQ(center.y, 6.0f);
  EXPECT_FLOAT_EQ(center.z, 8.0f);
}

TEST(AABB, GivenUnitBox_WhenExtentsCalculated_ThenReturnsHalfDimensions)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::vec3 extents = box.extents();

  EXPECT_FLOAT_EQ(extents.x, 1.0f);
  EXPECT_FLOAT_EQ(extents.y, 1.0f);
  EXPECT_FLOAT_EQ(extents.z, 1.0f);
}

TEST(AABB, GivenAsymmetricBox_WhenExtentsCalculated_ThenReturnsCorrectExtents)
{
  AABB box;
  box.expand(glm::vec3(0.0f, 0.0f, 0.0f));
  box.expand(glm::vec3(4.0f, 6.0f, 8.0f));

  glm::vec3 extents = box.extents();

  EXPECT_FLOAT_EQ(extents.x, 2.0f);
  EXPECT_FLOAT_EQ(extents.y, 3.0f);
  EXPECT_FLOAT_EQ(extents.z, 4.0f);
}

// =============================================================================
// AABB isValid Tests
// =============================================================================

TEST(AABB, GivenValidBox_WhenIsValidCalled_ThenReturnsTrue)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  EXPECT_TRUE(box.isValid());
}

TEST(AABB, GivenPointBox_WhenIsValidCalled_ThenReturnsTrue)
{
  AABB box;
  box.expand(glm::vec3(5.0f, 5.0f, 5.0f));

  EXPECT_TRUE(box.isValid());
}

// =============================================================================
// transformAABB Tests
// =============================================================================

TEST(transformAABB, GivenIdentityTransform_WhenTransformed_ThenBoxUnchanged)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  AABB result = transformAABB(box, glm::mat4(1.0f));

  EXPECT_NEAR(result.min.x, -1.0f, 1e-5f);
  EXPECT_NEAR(result.min.y, -1.0f, 1e-5f);
  EXPECT_NEAR(result.min.z, -1.0f, 1e-5f);
  EXPECT_NEAR(result.max.x, 1.0f, 1e-5f);
  EXPECT_NEAR(result.max.y, 1.0f, 1e-5f);
  EXPECT_NEAR(result.max.z, 1.0f, 1e-5f);
}

TEST(transformAABB, GivenTranslation_WhenTransformed_ThenBoxTranslated)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
  AABB      result    = transformAABB(box, translate);

  EXPECT_NEAR(result.min.x, 9.0f, 1e-5f);
  EXPECT_NEAR(result.min.y, 19.0f, 1e-5f);
  EXPECT_NEAR(result.min.z, 29.0f, 1e-5f);
  EXPECT_NEAR(result.max.x, 11.0f, 1e-5f);
  EXPECT_NEAR(result.max.y, 21.0f, 1e-5f);
  EXPECT_NEAR(result.max.z, 31.0f, 1e-5f);
}

TEST(transformAABB, GivenUniformScale_WhenTransformed_ThenBoxScaled)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::mat4 scale  = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f));
  AABB      result = transformAABB(box, scale);

  EXPECT_NEAR(result.min.x, -2.0f, 1e-5f);
  EXPECT_NEAR(result.min.y, -2.0f, 1e-5f);
  EXPECT_NEAR(result.min.z, -2.0f, 1e-5f);
  EXPECT_NEAR(result.max.x, 2.0f, 1e-5f);
  EXPECT_NEAR(result.max.y, 2.0f, 1e-5f);
  EXPECT_NEAR(result.max.z, 2.0f, 1e-5f);
}

TEST(transformAABB, GivenNonUniformScale_WhenTransformed_ThenBoxNonUniformlyScaled)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, -1.0f, -1.0f));
  box.expand(glm::vec3(1.0f, 1.0f, 1.0f));

  glm::mat4 scale  = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
  AABB      result = transformAABB(box, scale);

  EXPECT_NEAR(result.min.x, -1.0f, 1e-5f);
  EXPECT_NEAR(result.min.y, -2.0f, 1e-5f);
  EXPECT_NEAR(result.min.z, -3.0f, 1e-5f);
  EXPECT_NEAR(result.max.x, 1.0f, 1e-5f);
  EXPECT_NEAR(result.max.y, 2.0f, 1e-5f);
  EXPECT_NEAR(result.max.z, 3.0f, 1e-5f);
}

TEST(transformAABB, Given90DegreeRotation_WhenTransformed_ThenBoxRotated)
{
  AABB box;
  box.expand(glm::vec3(-1.0f, 0.0f, 0.0f));
  box.expand(glm::vec3(1.0f, 0.5f, 0.5f));

  // Rotate 90 degrees around Y axis: X -> Z, Z -> -X
  glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  AABB      result = transformAABB(box, rotate);

  // After 90 degree Y rotation, the box should have swapped X and Z dimensions
  EXPECT_TRUE(result.isValid());
  // New Z should be from old X (-1 to 1)
  EXPECT_NEAR(result.min.z, -1.0f, 1e-5f);
  EXPECT_NEAR(result.max.z, 1.0f, 1e-5f);
}

// =============================================================================
// Model::Vertex Tests
// =============================================================================

TEST(ModelVertex, GivenTwoIdenticalVertices_WhenCompared_ThenAreEqual)
{
  Model::Vertex v1{.position = glm::vec3(1.0f, 2.0f, 3.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f, 0.5f)};
  Model::Vertex v2{.position = glm::vec3(1.0f, 2.0f, 3.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f, 0.5f)};

  EXPECT_TRUE(v1 == v2);
}

TEST(ModelVertex, GivenVerticesWithDifferentPositions_WhenCompared_ThenAreNotEqual)
{
  Model::Vertex v1{.position = glm::vec3(1.0f, 2.0f, 3.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f)};
  Model::Vertex v2{.position = glm::vec3(1.0f, 2.0f, 4.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f)};

  EXPECT_FALSE(v1 == v2);
}

TEST(ModelVertex, GivenVerticesWithDifferentColors_WhenCompared_ThenAreNotEqual)
{
  Model::Vertex v1{.position = glm::vec3(1.0f), .color = glm::vec3(1.0f, 0.0f, 0.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f)};
  Model::Vertex v2{.position = glm::vec3(1.0f), .color = glm::vec3(0.0f, 1.0f, 0.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f)};

  EXPECT_FALSE(v1 == v2);
}

TEST(ModelVertex, GivenVerticesWithDifferentNormals_WhenCompared_ThenAreNotEqual)
{
  Model::Vertex v1{.position = glm::vec3(1.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f)};
  Model::Vertex v2{.position = glm::vec3(1.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(1.0f, 0.0f, 0.0f), .uv = glm::vec2(0.5f)};

  EXPECT_FALSE(v1 == v2);
}

TEST(ModelVertex, GivenVerticesWithDifferentUVs_WhenCompared_ThenAreNotEqual)
{
  Model::Vertex v1{.position = glm::vec3(1.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.5f, 0.5f)};
  Model::Vertex v2{.position = glm::vec3(1.0f), .color = glm::vec3(1.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f), .uv = glm::vec2(0.0f, 0.0f)};

  EXPECT_FALSE(v1 == v2);
}

// =============================================================================
// Model::Node Tests
// =============================================================================

TEST(ModelNode, GivenDefaultNode_WhenInspected_ThenHasIdentityTransform)
{
  Model::Node node;

  EXPECT_EQ(node.translation, glm::vec3(0.0f));
  EXPECT_EQ(node.scale, glm::vec3(1.0f));
  EXPECT_EQ(node.rotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
  EXPECT_EQ(node.matrix, glm::mat4(1.0f));
}

TEST(ModelNode, GivenDefaultNode_WhenLocalTransformCalculated_ThenIsIdentity)
{
  Model::Node node;
  glm::mat4   transform = node.getLocalTransform();

  glm::mat4 identity = glm::mat4(1.0f);
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      EXPECT_NEAR(transform[i][j], identity[i][j], 1e-5f);
    }
  }
}

TEST(ModelNode, GivenNodeWithTranslation_WhenLocalTransformCalculated_ThenTranslationApplied)
{
  Model::Node node;
  node.translation = glm::vec3(10.0f, 20.0f, 30.0f);

  glm::mat4 transform = node.getLocalTransform();
  glm::vec4 origin    = transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  EXPECT_NEAR(origin.x, 10.0f, 1e-5f);
  EXPECT_NEAR(origin.y, 20.0f, 1e-5f);
  EXPECT_NEAR(origin.z, 30.0f, 1e-5f);
}

TEST(ModelNode, GivenNodeWithScale_WhenLocalTransformCalculated_ThenScaleApplied)
{
  Model::Node node;
  node.scale = glm::vec3(2.0f, 3.0f, 4.0f);

  glm::mat4 transform = node.getLocalTransform();
  glm::vec4 point     = transform * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

  EXPECT_NEAR(point.x, 2.0f, 1e-5f);
  EXPECT_NEAR(point.y, 3.0f, 1e-5f);
  EXPECT_NEAR(point.z, 4.0f, 1e-5f);
}

TEST(ModelNode, GivenNodeWithMesh_WhenInspected_ThenMeshIndexIsCorrect)
{
  Model::Node node;
  node.mesh = 5;

  EXPECT_EQ(node.mesh, 5);
}

TEST(ModelNode, GivenNodeWithChildren_WhenInspected_ThenChildrenAccessible)
{
  Model::Node node;
  node.children = {1, 2, 3};

  EXPECT_EQ(node.children.size(), 3);
  EXPECT_EQ(node.children[0], 1);
  EXPECT_EQ(node.children[1], 2);
  EXPECT_EQ(node.children[2], 3);
}

// =============================================================================
// Model::MeshletBuildConfig Tests
// =============================================================================

TEST(MeshletBuildConfig, GivenDefaultConfig_WhenInspected_ThenHasReasonableDefaults)
{
  Model::MeshletBuildConfig config;

  EXPECT_EQ(config.maxVertices, 64);
  EXPECT_EQ(config.maxTriangles, 124);
  EXPECT_FLOAT_EQ(config.coneWeight, 0.0f);
  EXPECT_FLOAT_EQ(config.maxRadius, 0.0f);
}

TEST(MeshletBuildConfig, GivenCustomConfig_WhenSet_ThenValuesAreStored)
{
  Model::MeshletBuildConfig config;
  config.maxVertices  = 128;
  config.maxTriangles = 256;
  config.coneWeight   = 0.5f;
  config.maxRadius    = 2.0f;

  EXPECT_EQ(config.maxVertices, 128);
  EXPECT_EQ(config.maxTriangles, 256);
  EXPECT_FLOAT_EQ(config.coneWeight, 0.5f);
  EXPECT_FLOAT_EQ(config.maxRadius, 2.0f);
}

// =============================================================================
// Model::AnimationSampler Interpolation Tests
// =============================================================================

TEST(AnimationSampler, GivenDefaultSampler_WhenInspected_ThenInterpolationIsLinear)
{
  Model::AnimationSampler sampler;

  EXPECT_EQ(sampler.interpolation, Model::AnimationSampler::LINEAR);
}

TEST(AnimationSampler, GivenSamplerWithStepInterpolation_WhenSet_ThenValueIsStored)
{
  Model::AnimationSampler sampler;
  sampler.interpolation = Model::AnimationSampler::STEP;

  EXPECT_EQ(sampler.interpolation, Model::AnimationSampler::STEP);
}

TEST(AnimationSampler, GivenSamplerWithCubicSpline_WhenSet_ThenValueIsStored)
{
  Model::AnimationSampler sampler;
  sampler.interpolation = Model::AnimationSampler::CUBICSPLINE;

  EXPECT_EQ(sampler.interpolation, Model::AnimationSampler::CUBICSPLINE);
}

// =============================================================================
// Model::AnimationChannel Tests
// =============================================================================

TEST(AnimationChannel, GivenTranslationChannel_WhenSet_ThenValuesAreStored)
{
  Model::AnimationChannel channel;
  channel.targetNode   = 5;
  channel.path         = Model::AnimationChannel::TRANSLATION;
  channel.samplerIndex = 0;

  EXPECT_EQ(channel.targetNode, 5);
  EXPECT_EQ(channel.path, Model::AnimationChannel::TRANSLATION);
  EXPECT_EQ(channel.samplerIndex, 0);
}

TEST(AnimationChannel, GivenRotationChannel_WhenSet_ThenValuesAreStored)
{
  Model::AnimationChannel channel;
  channel.path = Model::AnimationChannel::ROTATION;

  EXPECT_EQ(channel.path, Model::AnimationChannel::ROTATION);
}

TEST(AnimationChannel, GivenScaleChannel_WhenSet_ThenValuesAreStored)
{
  Model::AnimationChannel channel;
  channel.path = Model::AnimationChannel::SCALE;

  EXPECT_EQ(channel.path, Model::AnimationChannel::SCALE);
}

TEST(AnimationChannel, GivenWeightsChannel_WhenSet_ThenValuesAreStored)
{
  Model::AnimationChannel channel;
  channel.path = Model::AnimationChannel::WEIGHTS;

  EXPECT_EQ(channel.path, Model::AnimationChannel::WEIGHTS);
}

// =============================================================================
// Model::Animation Tests
// =============================================================================

TEST(Animation, GivenDefaultAnimation_WhenInspected_ThenDurationIsZero)
{
  Model::Animation anim;

  EXPECT_FLOAT_EQ(anim.duration, 0.0f);
  EXPECT_TRUE(anim.name.empty());
  EXPECT_TRUE(anim.channels.empty());
  EXPECT_TRUE(anim.samplers.empty());
}

TEST(Animation, GivenAnimationWithData_WhenSet_ThenValuesAreStored)
{
  Model::Animation anim;
  anim.name     = "walk";
  anim.duration = 2.5f;
  anim.channels.resize(2);
  anim.samplers.resize(2);

  EXPECT_EQ(anim.name, "walk");
  EXPECT_FLOAT_EQ(anim.duration, 2.5f);
  EXPECT_EQ(anim.channels.size(), 2);
  EXPECT_EQ(anim.samplers.size(), 2);
}

// =============================================================================
// Model::SubMesh Tests
// =============================================================================

TEST(SubMesh, GivenSubMesh_WhenConfigured_ThenValuesAreStored)
{
  Model::SubMesh subMesh;
  subMesh.indexOffset   = 100;
  subMesh.indexCount    = 300;
  subMesh.materialId    = 2;
  subMesh.meshletOffset = 10;
  subMesh.meshletCount  = 5;

  EXPECT_EQ(subMesh.indexOffset, 100);
  EXPECT_EQ(subMesh.indexCount, 300);
  EXPECT_EQ(subMesh.materialId, 2);
  EXPECT_EQ(subMesh.meshletOffset, 10);
  EXPECT_EQ(subMesh.meshletCount, 5);
}
