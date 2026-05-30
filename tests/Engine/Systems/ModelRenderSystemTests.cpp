#include <gtest/gtest.h>

#include <cstring>
#include <thread>
#include <vector>

#include "../../fixtures/DeviceFixture.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

using namespace engine;

// =============================================================================
// MeshPushConstantData Tests
// =============================================================================

TEST(MeshPushConstantData, GivenDefaultConstructed_WhenInspected_ThenAllFieldsAreZeroInitialized) {
  MeshPushConstantData pcd{};

  EXPECT_EQ(pcd.meshId, 0u);
  EXPECT_EQ(pcd.meshletBufferAddress, 0u);
  EXPECT_EQ(pcd.meshletVerticesAddress, 0u);
  EXPECT_EQ(pcd.meshletTrianglesAddress, 0u);
  EXPECT_EQ(pcd.vertexBufferAddress, 0u);
  EXPECT_EQ(pcd.meshletOffset, 0u);
  EXPECT_EQ(pcd.meshletCount, 0u);

  // Model and normal matrices should be identity
  glm::mat4 identity = glm::mat4(1.0f);
  EXPECT_EQ(pcd.modelMatrix, identity);
  EXPECT_EQ(pcd.normalMatrix, identity);
}

TEST(MeshPushConstantData, GivenPushConstantStruct_WhenComparedToBinaryLayout_ThenOffsetsMatchShaderExpectations) {
  // This test ensures the memory layout matches what the shader expects.
  // If this fails, the shader/CPU side is misaligned.
  static_assert(sizeof(MeshPushConstantData) == 176, "Push constant size mismatch");
  static_assert(offsetof(MeshPushConstantData, modelMatrix) == 0, "modelMatrix offset mismatch");
  static_assert(offsetof(MeshPushConstantData, normalMatrix) == 64, "normalMatrix offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshId) == 128, "meshId offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshletBufferAddress) == 136, "meshletBufferAddress offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshletVerticesAddress) == 144, "meshletVerticesAddress offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshletTrianglesAddress) == 152, "meshletTrianglesAddress offset mismatch");
  static_assert(offsetof(MeshPushConstantData, vertexBufferAddress) == 160, "vertexBufferAddress offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshletOffset) == 168, "meshletOffset offset mismatch");
  static_assert(offsetof(MeshPushConstantData, meshletCount) == 172, "meshletCount offset mismatch");
  SUCCEED();
}

// =============================================================================
// ModelRenderSystem Multi-threading API Tests
// =============================================================================

TEST(ModelRenderSystem, GivenDefaultState_WhenEnablingMultiThreadedRecording_ThenApiAcceptsConfiguration) {
  // Basic API/behavior smoke: enabling/disabling should be idempotent and
  // accept a thread count. This test does NOT exercise GPU recording.
  Window win(16, 16, "MT Recording API");
  Device device(win);

  // Create a ModelRenderSystem with a null render pass (sufficient for API-only test)
  ModelRenderSystem mrs(device, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

  // Default: disabled
  mrs.enableMultiThreadedRecording(true, 4);
  mrs.enableMultiThreadedRecording(false, 0);

  // Re-enable with auto thread-count
  mrs.enableMultiThreadedRecording(true, 0);

  SUCCEED();
}

// =============================================================================
// MaterialRenderBindings Multi-threaded Tests
// =============================================================================

class MaterialRenderBindingsTest : public engine::test::DeviceFixtureWithSetup {
 protected:
  void SetUp() override {
    device().enableThreadLocalCommandPools();
  }
};

TEST_F(MaterialRenderBindingsTest, GivenResourcesCreated_WhenFrameBegins_ThenDescriptorSetIsValid) {
  MaterialRenderBindings mrb(device());
  mrb.createResources();
  mrb.beginFrame(0);

  EXPECT_TRUE(mrb.frameDescriptorSetValid(0));
  EXPECT_NE(mrb.getFrameDescriptorSet(0), VK_NULL_HANDLE);
}

TEST_F(MaterialRenderBindingsTest, GivenSerialRecording_WhenBindMaterialCalled_ThenCapturedHandleMatchesFrameSet) {
  MaterialRenderBindings mrb(device());
  mrb.createResources();
  mrb.beginFrame(0);

  VkDescriptorSet expectedHandle = mrb.getFrameDescriptorSet(0);

  // Build a pipeline layout with material set at index 4
  VkDescriptorSetLayout dummyLayouts[4] = {};
  for (int i = 0; i < 4; ++i) {
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 0;
    li.pBindings = nullptr;
    ASSERT_EQ(vkCreateDescriptorSetLayout(device().device(), &li, nullptr, &dummyLayouts[i]), VK_SUCCESS);
  }

  std::array<VkDescriptorSetLayout, 5> layouts{dummyLayouts[0], dummyLayouts[1], dummyLayouts[2], dummyLayouts[3], mrb.getDescriptorSetLayout()};

  VkPipelineLayoutCreateInfo pli{};
  pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pli.setLayoutCount = static_cast<uint32_t>(layouts.size());
  pli.pSetLayouts = layouts.data();

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  ASSERT_EQ(vkCreatePipelineLayout(device().device(), &pli, nullptr, &pipelineLayout), VK_SUCCESS);

  // Allocate and begin a secondary command buffer
  VkCommandBuffer serialCB = VK_NULL_HANDLE;
  ASSERT_EQ(device().allocateSecondaryCommandBuffer(&serialCB), VK_SUCCESS);

  VkCommandBufferInheritanceRenderingInfoKHR renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
  renderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkCommandBufferInheritanceInfo inherit{};
  inherit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inherit.pNext = &renderingInfo;

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  bi.pInheritanceInfo = &inherit;

  ASSERT_EQ(vkBeginCommandBuffer(serialCB, &bi), VK_SUCCESS);

  Camera camera;
  Scene scene;
  FrameInfo frameInfo{0, 0.0f, serialCB, camera, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {8, 8}, 0};

  mrb.enableBindCapture(true);
  mrb.bindMaterial(frameInfo, pipelineLayout, nullptr, 0.0f);
  ASSERT_EQ(vkEndCommandBuffer(serialCB), VK_SUCCESS);

  auto captured = mrb.getCapturedBinds();
  ASSERT_EQ(captured.size(), 1u);
  EXPECT_EQ(captured[0], expectedHandle);
  EXPECT_NE(captured[0], VK_NULL_HANDLE);

  // Cleanup
  mrb.enableBindCapture(false);
  device().freeSecondaryCommandBuffer(serialCB);
  vkDestroyPipelineLayout(device().device(), pipelineLayout, nullptr);
  for (auto& layout : dummyLayouts) {
    vkDestroyDescriptorSetLayout(device().device(), layout, nullptr);
  }
}

TEST_F(MaterialRenderBindingsTest, GivenMultipleWorkerThreads_WhenBindMaterialCalled_ThenAllCapturedHandlesMatchSerialHandle) {
  MaterialRenderBindings mrb(device());
  mrb.createResources();
  mrb.beginFrame(0);

  VkDescriptorSet serialHandle = mrb.getFrameDescriptorSet(0);

  // Build pipeline layout
  VkDescriptorSetLayout dummyLayouts[4] = {};
  for (int i = 0; i < 4; ++i) {
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 0;
    li.pBindings = nullptr;
    ASSERT_EQ(vkCreateDescriptorSetLayout(device().device(), &li, nullptr, &dummyLayouts[i]), VK_SUCCESS);
  }

  std::array<VkDescriptorSetLayout, 5> layouts{dummyLayouts[0], dummyLayouts[1], dummyLayouts[2], dummyLayouts[3], mrb.getDescriptorSetLayout()};

  VkPipelineLayoutCreateInfo pli{};
  pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pli.setLayoutCount = static_cast<uint32_t>(layouts.size());
  pli.pSetLayouts = layouts.data();

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  ASSERT_EQ(vkCreatePipelineLayout(device().device(), &pli, nullptr, &pipelineLayout), VK_SUCCESS);

  auto beginSecondary = [](VkCommandBuffer cb) -> VkResult {
    VkCommandBufferInheritanceRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
    renderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkCommandBufferInheritanceInfo inherit{};
    inherit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit.pNext = &renderingInfo;

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    bi.pInheritanceInfo = &inherit;

    return vkBeginCommandBuffer(cb, &bi);
  };

  // Serial bind first
  VkCommandBuffer serialCB = VK_NULL_HANDLE;
  ASSERT_EQ(device().allocateSecondaryCommandBuffer(&serialCB), VK_SUCCESS);
  ASSERT_EQ(beginSecondary(serialCB), VK_SUCCESS);

  Camera camera;
  Scene scene;
  FrameInfo frameInfo{0, 0.0f, serialCB, camera, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {8, 8}, 0};

  mrb.enableBindCapture(true);
  mrb.bindMaterial(frameInfo, pipelineLayout, nullptr, 0.0f);
  ASSERT_EQ(vkEndCommandBuffer(serialCB), VK_SUCCESS);

  // Worker threads
  const int workerCount = 2;
  std::vector<VkCommandBuffer> workerCbs;
  std::mutex workerCbsMutex;
  std::vector<std::thread> workers;

  for (int t = 0; t < workerCount; ++t) {
    workers.emplace_back([this, &mrb, pipelineLayout, &scene, &workerCbs, &workerCbsMutex, &beginSecondary]() {
      Camera cam;
      VkCommandBuffer localCb = VK_NULL_HANDLE;

      if (device().allocateSecondaryCommandBuffer(&localCb) != VK_SUCCESS) {
        return;
      }

      if (beginSecondary(localCb) == VK_SUCCESS) {
        FrameInfo localFrame{0, 0.0f, localCb, cam, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {8, 8}, 0};
        mrb.bindMaterial(localFrame, pipelineLayout, nullptr, 0.0f);
        vkEndCommandBuffer(localCb);
      }

      {
        std::lock_guard<std::mutex> lk(workerCbsMutex);
        workerCbs.push_back(localCb);
      }
    });
  }

  for (auto& w : workers) {
    if (w.joinable()) w.join();
  }

  auto mtCaptured = mrb.getCapturedBinds();
  ASSERT_EQ(mtCaptured.size(), static_cast<size_t>(1 + workerCount));

  for (size_t i = 1; i < mtCaptured.size(); ++i) {
    EXPECT_EQ(mtCaptured[i], serialHandle) << "Worker bind at index " << i << " differs from serial";
    EXPECT_NE(mtCaptured[i], VK_NULL_HANDLE) << "Worker bind at index " << i << " is VK_NULL_HANDLE";
  }

  // Cleanup
  mrb.enableBindCapture(false);
  for (auto cb : workerCbs) {
    device().freeSecondaryCommandBuffer(cb);
  }
  device().freeSecondaryCommandBuffer(serialCB);
  vkDestroyPipelineLayout(device().device(), pipelineLayout, nullptr);
  for (auto& layout : dummyLayouts) {
    vkDestroyDescriptorSetLayout(device().device(), layout, nullptr);
  }
}
