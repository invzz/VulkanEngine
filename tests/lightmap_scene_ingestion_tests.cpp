#include <gtest/gtest.h>

#include <fstream>

#include "Tools/LightmapBakerLib/Scene.hpp"

using namespace LightmapBaker;

TEST(LightmapSceneIngestion, ParsesSimpleScene)
{
  const char*   tmp = "/tmp/test_scene_ingest.json";
  const char*   txt = R"JSON({
    "objects": [
      {
        "directionalLight": { "bake": true, "color": [1,1,1], "intensity": 1.0 },
        "id": "light_1",
        "transform": { "rotation": [0,0,0], "translation": [0,0,0], "scale": [1,1,1] }
      },
      {
        "id": "obj_1",
        "name": "TestObj",
        "modelPath": "assets/models/test.obj",
        "transform": { "rotation": [0,0,0], "translation": [1,2,3], "scale": [1,1,1] }
      }
    ]
  })JSON";
  std::ofstream out(tmp);
  out << txt;
  out.close();

  Scene       s;
  std::string err;
  ASSERT_TRUE(s.loadFromFile(tmp, &err)) << err;

  ASSERT_EQ(s.lights.size(), 1);
  EXPECT_TRUE(s.lights[0].bake);
  EXPECT_EQ(s.objects.size(), 2);
  EXPECT_EQ(s.objects[1].id, "obj_1");
  EXPECT_TRUE(s.objects[1].modelPath.has_value());
  EXPECT_EQ(s.objects[1].modelPath.value(), std::string("assets/models/test.obj"));
}
TEST(LightmapSceneIngestion, NumericIdAndMeshKey)
{
  const char*   tmp = "/tmp/test_scene_ingest_mesh.json";
  const char*   txt = R"JSON({
    "objects": [
      { "id": 123, "mesh": "assets/models/mesh.obj" }
    ]
  })JSON";
  std::ofstream out(tmp);
  out << txt;
  out.close();

  Scene       s;
  std::string err;
  ASSERT_TRUE(s.loadFromFile(tmp, &err)) << err;

  ASSERT_EQ(s.objects.size(), 1);
  EXPECT_EQ(s.objects[0].id, "123");
  ASSERT_TRUE(s.objects[0].modelPath.has_value());
  EXPECT_EQ(s.objects[0].modelPath.value(), std::string("assets/models/mesh.obj"));
}

TEST(LightmapSceneIngestion, DirectionalLightDirectionFromRotation)
{
  const char*   tmp = "/tmp/test_scene_light_dir.json";
  const char*   txt = R"JSON({
    "objects": [
      {
        "directionalLight": { "bake": true },
        "transform": { "rotation": [0,0,1,0] }
      }
    ]
  })JSON";
  std::ofstream out(tmp);
  out << txt;
  out.close();

  Scene       s;
  std::string err;
  ASSERT_TRUE(s.loadFromFile(tmp, &err)) << err;

  ASSERT_EQ(s.lights.size(), 1);
  auto dir = s.lights[0].direction;
  EXPECT_NEAR(dir.x, 0.0f, 1e-6);
  EXPECT_NEAR(dir.y, 1.0f, 1e-6);
  EXPECT_NEAR(dir.z, 0.0f, 1e-6);
}
