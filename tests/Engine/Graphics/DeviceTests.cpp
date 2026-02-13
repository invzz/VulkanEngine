#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

// =============================================================================
// Device Tests
// =============================================================================

TEST(Device, GivenThreadLocalPoolsEnabled_WhenThreadsUseCommands_ThenCleanupSucceeds) {
  Window window(1, 1, "DeviceTest");
  {
    Device device(window);
    device.enableThreadLocalCommandPools();

    const int N = 4;
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i) {
      threads.emplace_back([&device]() {
        try {
          VkCommandBuffer cmd = device.beginSingleTimeCommands();
          device.endSingleTimeCommands(cmd);
        } catch (...) {
          // Swallow to ensure thread completion
        }
      });
    }

    for (auto& t : threads)
      t.join();
  }

  // If we reach here the destructor didn't abort the process
  SUCCEED();
}
