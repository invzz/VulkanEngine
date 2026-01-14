#include <gtest/gtest.h>

#include "Engine/Graphics/ExtensionHelpers.hpp"

using namespace engine;

TEST(ExtensionHelpers, EnsureExtensionsPresent_AllPresent)
{
  std::vector<VkExtensionProperties> available = {
          VkExtensionProperties{"VK_EXT_one", 0},
          VkExtensionProperties{"VK_KHR_swapchain", 0},
          VkExtensionProperties{"VK_EXT_debug_utils", 0},
  };

  std::vector<const char*> required = {"VK_EXT_one", "VK_KHR_swapchain"};
  EXPECT_TRUE(ensureExtensionsPresent(required, available));
}

TEST(ExtensionHelpers, EnsureExtensionsPresent_Missing)
{
  std::vector<VkExtensionProperties> available = {
          VkExtensionProperties{"VK_EXT_one", 0},
          VkExtensionProperties{"VK_EXT_debug_utils", 0},
  };

  std::vector<const char*> required = {"VK_EXT_one", "VK_KHR_swapchain"};
  EXPECT_FALSE(ensureExtensionsPresent(required, available));
}
