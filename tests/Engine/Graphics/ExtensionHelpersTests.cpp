#include <gtest/gtest.h>

#include "Engine/Graphics/ExtensionHelpers.hpp"

using namespace engine;

// =============================================================================
// ExtensionHelpers Tests
// =============================================================================

TEST(ExtensionHelpers, GivenAllExtensionsAvailable_WhenEnsurePresent_ThenReturnsTrue) {
    std::vector<VkExtensionProperties> available = {
        VkExtensionProperties{"VK_EXT_one", 0},
        VkExtensionProperties{"VK_KHR_swapchain", 0},
        VkExtensionProperties{"VK_EXT_debug_utils", 0},
    };

    std::vector<const char*> required = {"VK_EXT_one", "VK_KHR_swapchain"};
    EXPECT_TRUE(ensureExtensionsPresent(required, available));
}

TEST(ExtensionHelpers, GivenMissingExtension_WhenEnsurePresent_ThenReturnsFalse) {
    std::vector<VkExtensionProperties> available = {
        VkExtensionProperties{"VK_EXT_one", 0},
        VkExtensionProperties{"VK_EXT_debug_utils", 0},
    };

    std::vector<const char*> required = {"VK_EXT_one", "VK_KHR_swapchain"};
    EXPECT_FALSE(ensureExtensionsPresent(required, available));
}
