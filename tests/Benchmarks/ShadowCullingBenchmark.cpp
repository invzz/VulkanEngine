#include <gtest/gtest.h>

#include <chrono>
#include <iostream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "ModelLib/Resources/Model.hpp"

using namespace engine;

class ShadowCullingBenchmark : public ::testing::Test {
 protected:
  void SetUp() override {
    window = std::make_unique<Window>(1, 1, "ShadowCulling Benchmark");
    device = std::make_unique<Device>(*window);
  }

  void TearDown() override {
    device->WaitIdle();
    device.reset();
    window.reset();
  }

  std::unique_ptr<Window> window;
  std::unique_ptr<Device> device;
};

// Disabled by default because timings are CI-flaky; useful for local profiling.
TEST_F(ShadowCullingBenchmark, DISABLED_SimpleCullingComparison_PrintTimings) {
  Scene scene;
  Camera camera;
  camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 200.0f);
  camera.setViewYXZ(glm::vec3(0.0f), glm::vec3(0.0f));

  // Create a tiny procedural model (single triangle) via Builder
  Model::Builder builder;
  Model::Vertex v0{{-0.5f, -0.5f, 0.0f}, {}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}};
  Model::Vertex v1{{0.5f, -0.5f, 0.0f}, {}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}};
  Model::Vertex v2{{0.0f, 0.5f, 0.0f}, {}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}};
  builder.vertices = {v0, v1, v2};
  builder.indices = {0u, 1u, 2u};
  Model::SubMesh sm{};
  sm.indexOffset = 0;
  sm.indexCount = 3;
  sm.materialId = -1;
  builder.subMeshes.push_back(sm);

  auto modelPtr = std::make_shared<Model>(*device, builder);

  // Spawn many entities that reference the same model
  const int instanceCount = 800;
  for (int i = 0; i < instanceCount; ++i) {
    auto e = scene.createEntity();
    auto& mc = scene.getRegistry().emplace<ModelComponent>(e);
    mc.model = modelPtr;
    auto& t = scene.getRegistry().emplace<TransformComponent>(e);
    t.translation = glm::vec3((i % 40) * 2.0f - 40.0f, 0.0f, (i / 40) * 2.0f - 40.0f);
  }

  // Add a directional light
  {
    auto e = scene.createEntity();
    auto& dl = scene.getRegistry().emplace<DirectionalLightComponent>(e);
    dl.color = glm::vec3(1.0f);
    dl.intensity = 1.0f;
    scene.getRegistry().emplace<TransformComponent>(e).rotation = glm::vec3(glm::radians(-45.0f), 0.0f, 0.0f);
  }

  // Prepare ShadowSystem
  ShadowSystem shadowSystem(*device, 1024);
  ShadowSettings settings;

  FrameInfo frameInfo{
      .frameIndex = 0,
      .frameTime = 0.0f,
      .commandBuffer = VK_NULL_HANDLE,
      .camera = camera,
      .globalDescriptorSet = VK_NULL_HANDLE,
      .globalTextureSet = VK_NULL_HANDLE,
      .scene = &scene,
      .selectedObjectId = 0,
      .selectedEntity = entt::null,
      .cameraEntity = entt::null,
      .morphManager = nullptr,
      .extent = {1, 1},
      .debugMode = 0,
  };

  // Warm up
  VkCommandBuffer cmd = device->beginSingleTimeCommands();
  frameInfo.commandBuffer = cmd;
  shadowSystem.renderShadowMaps(frameInfo, settings);
  device->endSingleTimeCommands(cmd);

  auto measure = [&](bool enableCulling) {
    settings.enableShadowCulling = enableCulling;
    auto start = std::chrono::high_resolution_clock::now();
    VkCommandBuffer cmd = device->beginSingleTimeCommands();
    frameInfo.commandBuffer = cmd;
    shadowSystem.renderShadowMaps(frameInfo, settings);
    device->endSingleTimeCommands(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  };

  long tNoCull = measure(false);
  long tCull = measure(true);

  std::cout << "Shadow culling benchmark (instances=" << instanceCount << ") -- noCull=" << tNoCull << "ms, cull=" << tCull << "ms\n";

  // Sanity: both should run and culling should not be slower by a large factor
  EXPECT_LT(tCull, tNoCull * 5 + 2000);
}
