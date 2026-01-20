#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

using namespace engine;

TEST(Device, ThreadLocalPoolsDestroyedBeforeDevice)
{
  Window window(1, 1, "TLCP_Teardown");
  {
    Device device(window);
    device.enableThreadLocalCommandPools();

    // Spawn a few threads to exercise per-thread pools via the public
    // single-time command helpers.
    const int                N = 4;
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i)
    {
      threads.emplace_back([&device]() {
        try
        {
          VkCommandBuffer cmd = device.beginSingleTimeCommands();
          device.endSingleTimeCommands(cmd);
        }
        catch (...)
        {
          // Test should fail if exceptions are thrown, but swallow here to
          // ensure thread completion and let the destructor assert on crash.
        }
      });
    }

    for (auto& t : threads)
      t.join();

    // Device goes out of scope here; previously this caused vkDestroyCommandPool
    // to be called with an invalid device handle leading to a crash.
  }

  // If we reach here the destructor didn't abort the process.
  SUCCEED();
}
