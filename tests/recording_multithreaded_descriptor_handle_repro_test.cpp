#include <gtest/gtest.h>

#include <thread>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"

using namespace engine;

// Minimal reproducer: ensure MaterialRenderBindings binds the same non-null
// VkDescriptorSet handle when invoked from a single-threaded (serial)
// secondary-command-buffer and when invoked concurrently from worker
// recorded secondary command buffers.
TEST(ModelRenderSystem, MaterialBindHandle_SerialVsWorkerCapture)
{
  Window win(8, 8, "MT-bind-handle-repro");
  Device device(win);

  // Enable thread-local command pools so worker threads allocate from
  // distinct pools (avoids threading errors on vkBegin/vkEnd).
  device.enableThreadLocalCommandPools();

  // Create the material bindings and allocate per-frame resources.
  MaterialRenderBindings mrb(device);
  mrb.createResources();
  mrb.beginFrame(0);

  // Sanity: ensure the per-frame descriptor set is valid after createResources
  ASSERT_TRUE(mrb.frameDescriptorSetValid(0));
  VkDescriptorSet serialHandle = mrb.getFrameDescriptorSet(0);
  ASSERT_NE(serialHandle, VK_NULL_HANDLE);

  // Build a pipeline layout that has the material set at index kMaterialSetIndex (4)
  // to make vkCmdBindDescriptorSets validation happy. Create 4 empty dummy set
  // layouts for indices 0..3 and use the material layout for index 4.
  VkDescriptorSetLayout dummyLayouts[4] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
  for (int i = 0; i < 4; ++i)
  {
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 0;
    li.pBindings    = nullptr;
    VkResult r      = vkCreateDescriptorSetLayout(device.device(), &li, nullptr, &dummyLayouts[i]);
    ASSERT_EQ(r, VK_SUCCESS);
  }

  std::array<VkDescriptorSetLayout, 5> layouts{};
  layouts[0] = dummyLayouts[0];
  layouts[1] = dummyLayouts[1];
  layouts[2] = dummyLayouts[2];
  layouts[3] = dummyLayouts[3];
  layouts[4] = mrb.getDescriptorSetLayout();

  VkPipelineLayoutCreateInfo pli{};
  pli.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pli.setLayoutCount = static_cast<uint32_t>(layouts.size());
  pli.pSetLayouts    = layouts.data();

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  ASSERT_EQ(vkCreatePipelineLayout(device.device(), &pli, nullptr, &pipelineLayout), VK_SUCCESS);

  // Helper: begin a secondary command buffer for recording a material bind.
  auto beginSecondary = [&](VkCommandBuffer cb) {
    VkCommandBufferInheritanceInfo inherit{};
    inherit.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit.renderPass  = VK_NULL_HANDLE;
    inherit.subpass     = 0;
    inherit.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferInheritanceRenderingInfoKHR renderingInfo{};
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
    renderingInfo.pNext                   = nullptr;
    renderingInfo.flags                   = 0;
    renderingInfo.viewMask                = 0;
    renderingInfo.colorAttachmentCount    = 0;
    renderingInfo.pColorAttachmentFormats = nullptr;
    renderingInfo.depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
    renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderingInfo.rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT;

    inherit.pNext = &renderingInfo;

    VkCommandBufferBeginInfo bi{};
    bi.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags            = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    bi.pInheritanceInfo = &inherit;

    ASSERT_EQ(vkBeginCommandBuffer(cb, &bi), VK_SUCCESS);
  };

  // Non-asserting variant for worker threads: returns VkResult so the worker
  // can record a thread-local success/failure without invoking gtest asserts
  // from a non-main thread.
  auto beginSecondaryNoAssert = [&](VkCommandBuffer cb) -> VkResult {
    VkCommandBufferInheritanceInfo inherit{};
    inherit.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit.renderPass  = VK_NULL_HANDLE;
    inherit.subpass     = 0;
    inherit.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferInheritanceRenderingInfoKHR renderingInfo{};
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
    renderingInfo.pNext                   = nullptr;
    renderingInfo.flags                   = 0;
    renderingInfo.viewMask                = 0;
    renderingInfo.colorAttachmentCount    = 0;
    renderingInfo.pColorAttachmentFormats = nullptr;
    renderingInfo.depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
    renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderingInfo.rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT;

    inherit.pNext = &renderingInfo;

    VkCommandBufferBeginInfo bi{};
    bi.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags            = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    bi.pInheritanceInfo = &inherit;

    return vkBeginCommandBuffer(cb, &bi);
  };

  auto endSecondary = [&](VkCommandBuffer cb) { ASSERT_EQ(vkEndCommandBuffer(cb), VK_SUCCESS); };

  // --- Serial binding (single secondary CB) ---
  VkCommandBuffer serialCB = VK_NULL_HANDLE;
  ASSERT_EQ(device.allocateSecondaryCommandBuffer(&serialCB), VK_SUCCESS);
  beginSecondary(serialCB);

  Camera    camera;
  Scene     scene;
  FrameInfo frameInfo{0, 0.0f, serialCB, camera, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {8, 8}, 0};

  // Enable capture so writeAndBind records the handle used
  mrb.enableBindCapture(true);
  mrb.bindMaterial(frameInfo, pipelineLayout, nullptr, 0.0f);
  endSecondary(serialCB);

  auto serialCaptured = mrb.getCapturedBinds();
  ASSERT_EQ(serialCaptured.size(), 1u);
  EXPECT_EQ(serialCaptured[0], serialHandle);
  EXPECT_NE(serialCaptured[0], VK_NULL_HANDLE);

  // --- Multithreaded simulation: two worker threads each record their own CB ---
  const int workerCount = 2;

  // Do NOT pre-allocate secondary CBs on the main thread — allocate them on the
  // worker threads (matches real usage and avoids command-pool threading errors).
  std::vector<VkCommandBuffer> workerCbs;
  std::mutex                   workerCbsMutex;

  // Launch workers and record per-worker begin/end status so we can diagnose failures.
  std::vector<std::thread> workers;
  std::vector<bool>        workerBeginOk(workerCount, false);
  std::vector<bool>        workerEndOk(workerCount, false);

  workers.reserve(workerCount);
  for (int t = 0; t < workerCount; ++t)
  {
    workers.emplace_back([&device, &mrb, pipelineLayout, &scene, t, &workerCbs, &workerCbsMutex, &beginSecondaryNoAssert, &workerBeginOk, &workerEndOk]() {
      Camera          cam;
      VkCommandBuffer localCb = VK_NULL_HANDLE;

      // Allocate on the worker thread (avoids cross-thread command-pool use!!!!)
      if (device.allocateSecondaryCommandBuffer(&localCb) != VK_SUCCESS)
      {
        return;
      }

      VkResult rBegin = beginSecondaryNoAssert(localCb);
      if (rBegin == VK_SUCCESS)
      {
        workerBeginOk[t] = true;

        FrameInfo localFrame{0, 0.0f, localCb, cam, VK_NULL_HANDLE, VK_NULL_HANDLE, &scene, 0, entt::null, entt::null, nullptr, {8, 8}, 0};
        mrb.bindMaterial(localFrame, pipelineLayout, nullptr, 0.0f);

        VkResult rEnd  = vkEndCommandBuffer(localCb);
        workerEndOk[t] = (rEnd == VK_SUCCESS);
      }

      // Record the worker's CB so the main thread can free it later
      {
        std::lock_guard<std::mutex> lk(workerCbsMutex);
        workerCbs.push_back(localCb);
      }
    });
  }

  for (auto& w : workers)
  {
    if (w.joinable()) w.join();
  }

  auto mtCaptured = mrb.getCapturedBinds();

  if (mtCaptured.size() != (1 + workerCount))
  {
    std::cerr << "[TEST DIAG] captured count=" << mtCaptured.size() << " expected=" << (1 + workerCount) << "\n";
    for (size_t i = 0; i < mtCaptured.size(); ++i)
    {
      std::cerr << "  captured[" << i << "]=" << mtCaptured[i] << "\n";
    }
    // A questo punto le liste dovrebbero essere uguali!
    for (int i = 0; i < workerCount; ++i)
    {
      std::cerr << "  worker[" << i << "] beginOk=" << workerBeginOk[i] << " endOk=" << workerEndOk[i] << "\n";
    }
  }

  // Expect exactly workerCount entries (one per bind) and all equal to the serial handle
  ASSERT_EQ(mtCaptured.size(), static_cast<size_t>(1 + workerCount)); // serial + worker captures are in the buffer

  // The first entry is the serial bind we did earlier; subsequent entries should match and be non-null
  for (size_t i = 1; i < mtCaptured.size(); ++i)
  {
    EXPECT_EQ(mtCaptured[i], serialHandle) << "mt bind at index " << i << " differs";
    EXPECT_NE(mtCaptured[i], VK_NULL_HANDLE) << "mt bind produced VK_NULL_HANDLE at index " << i;
  }

  // Cleanup
  for (auto cb : workerCbs)
  {
    device.freeSecondaryCommandBuffer(cb);
  }

  device.freeSecondaryCommandBuffer(serialCB);

  vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);

  for (auto& dummyLayout : dummyLayouts)
  {
    vkDestroyDescriptorSetLayout(device.device(), dummyLayout, nullptr);
  }

  mrb.enableBindCapture(false);
}
