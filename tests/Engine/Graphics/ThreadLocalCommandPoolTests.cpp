#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/ThreadLocalCommandPool.hpp"

using namespace engine;

TEST(ThreadLocalCommandPool, GivenInitializedPool_WhenGetForCurrentThread_ThenReturnsValidPool) {
    Window window(1, 1, "ThreadLocalCommandPoolTest");
    Device device(window);

    ThreadLocalCommandPool mgr;
    mgr.init(device.device(), device.findPhysicalQueueFamilies().graphicsFamily);

    VkCommandPool pool = mgr.getForCurrentThread();
    EXPECT_NE(pool, VK_NULL_HANDLE);

    mgr.destroyForCurrentThread();
}

TEST(ThreadLocalCommandPool, GivenValidPool_WhenCommandBufferAllocated_ThenSucceeds) {
    Window window(1, 1, "ThreadLocalCommandPoolTest");
    Device device(window);

    ThreadLocalCommandPool mgr;
    mgr.init(device.device(), device.findPhysicalQueueFamilies().graphicsFamily);

    VkCommandPool pool = mgr.getForCurrentThread();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    EXPECT_EQ(vkAllocateCommandBuffers(device.device(), &allocInfo, &cmd), VK_SUCCESS);
    EXPECT_NE(cmd, VK_NULL_HANDLE);

    vkFreeCommandBuffers(device.device(), pool, 1, &cmd);
    mgr.destroyForCurrentThread();
}

TEST(ThreadLocalCommandPool, GivenMultipleThreads_WhenAccessingConcurrently_ThenAllSucceed) {
    Window window(1, 1, "ThreadLocalCommandPoolTest");
    Device device(window);

    ThreadLocalCommandPool mgr;
    mgr.init(device.device(), device.findPhysicalQueueFamilies().graphicsFamily);

    const int                N = 8;
    std::vector<std::thread> threads;
    std::atomic<int>         successCount{0};

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&mgr, &device, &successCount]() {
            try {
                VkCommandPool               pool = mgr.getForCurrentThread();
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool        = pool;
                allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;

                VkCommandBuffer cmd = VK_NULL_HANDLE;
                if (vkAllocateCommandBuffers(device.device(), &allocInfo, &cmd) == VK_SUCCESS) {
                    vkFreeCommandBuffers(device.device(), pool, 1, &cmd);
                    successCount.fetch_add(1);
                }
            } catch (...) {
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(successCount.load(), N);

    mgr.destroyAll();
}

TEST(ThreadLocalCommandPool, GivenSingleThread_WhenManyAllocations_ThenPerformanceAcceptable) {
    Window window(1, 1, "ThreadLocalCommandPoolTest");
    Device device(window);

    ThreadLocalCommandPool mgr;
    mgr.init(device.device(), device.findPhysicalQueueFamilies().graphicsFamily);

    const int loops = 1000;
    auto      start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < loops; ++i) {
        VkCommandPool               pool = mgr.getForCurrentThread();
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device.device(), &allocInfo, &cmd);
        vkFreeCommandBuffers(device.device(), pool, 1, &cmd);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[TLCP] single-thread alloc/free loops=" << loops << " took " << dur << "ms\n";

    mgr.destroyAll();
}
