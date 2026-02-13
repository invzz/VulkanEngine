#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

class SingleTimeCommandBenchmarkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    window = std::make_unique<Window>(1, 1, "SingleTimeCommand Benchmark");
    device = std::make_unique<Device>(*window);
  }

  void TearDown() override {
    device->WaitIdle();
    device.reset();
    window.reset();
  }

  long runBenchmark(int threadCount, int loops) {
    auto worker = [this](int loops) {
      for (int i = 0; i < loops; ++i) {
        VkCommandBuffer cmd = device->beginSingleTimeCommands();
        // No real commands; just begin/end to measure allocation/free overhead
        device->endSingleTimeCommands(cmd);
      }
    };

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < threadCount; ++t)
      threads.emplace_back(worker, loops);
    for (auto& th : threads)
      th.join();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  }

  std::unique_ptr<Window> window;
  std::unique_ptr<Device> device;
};

TEST_F(SingleTimeCommandBenchmarkTest, GivenMultipleThreads_WhenComparingPoolStrategies_ThenThreadLocalIsNotDramaticallySlower) {
  const int threadCount = 8;
  const int loops = 100;

  // Warm-up
  runBenchmark(1, 10);

  // Per-call pools (default)
  long perCallMs = runBenchmark(threadCount, loops);
  std::cout << "Per-call pools: " << perCallMs << "ms" << std::endl;

  // Enable thread-local pools
  device->enableThreadLocalCommandPools();
  long threadLocalMs = runBenchmark(threadCount, loops);
  std::cout << "Thread-local pools: " << threadLocalMs << "ms" << std::endl;

  // Sanity check: thread-local should not be dramatically slower than per-call.
  EXPECT_LE(threadLocalMs, perCallMs * 5 + 1000);  // allow generous slack on CI

  // Print for developer info (not strict)
  std::cout << "Benchmark: per-call=" << perCallMs << "ms, thread-local=" << threadLocalMs << "ms\n";
}
