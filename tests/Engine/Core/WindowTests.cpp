#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"

using namespace engine;

// =============================================================================
// Window Construction Tests
// =============================================================================

TEST(Window, GivenValidDimensions_WhenConstructed_ThenWidthAndHeightAreCorrect) {
  Window window(800, 600, "Test Window");

  EXPECT_EQ(window.getWidth(), 800u);
  EXPECT_EQ(window.getHeight(), 600u);
}

TEST(Window, GivenValidDimensions_WhenGetExtent_ThenReturnsCorrectExtent) {
  Window window(1024, 768, "Extent Test");

  VkExtent2D extent = window.getExtent();

  EXPECT_EQ(extent.width, 1024u);
  EXPECT_EQ(extent.height, 768u);
}

TEST(Window, GivenSmallDimensions_WhenConstructed_ThenSucceeds) {
  // Minimal window size
  Window window(16, 16, "Small Window");

  EXPECT_EQ(window.getWidth(), 16u);
  EXPECT_EQ(window.getHeight(), 16u);
}

// =============================================================================
// Window GLFW Handle Tests
// =============================================================================

TEST(Window, GivenWindow_WhenGetGLFWwindow_ThenReturnsValidHandle) {
  Window window(64, 64, "GLFW Handle Test");

  GLFWwindow* glfwWindow = window.getGLFWwindow();
  EXPECT_NE(glfwWindow, nullptr);
}

// =============================================================================
// Window Resize Flag Tests
// =============================================================================

TEST(Window, GivenNewWindow_WhenWasWindowResized_ThenReturnsFalse) {
  Window window(64, 64, "Resize Flag Test");

  EXPECT_FALSE(window.wasWindowResized());
}

TEST(Window, GivenNewWindow_WhenConsumeWindowResized_ThenReturnsFalse) {
  Window window(64, 64, "Consume Resize Test");

  EXPECT_FALSE(window.consumeWindowResized());
}

TEST(Window, GivenWindow_WhenResetWindowResizedFlag_ThenFlagIsFalse) {
  Window window(64, 64, "Reset Resize Test");

  window.resetWindowResizedFlag();

  EXPECT_FALSE(window.wasWindowResized());
}

// =============================================================================
// Window Cursor Tests
// =============================================================================

TEST(Window, GivenNewWindow_WhenIsCursorVisible_ThenReturnsTrue) {
  Window window(64, 64, "Cursor Test");

  EXPECT_TRUE(window.isCursorVisible());
}

TEST(Window, GivenWindow_WhenSetCursorVisibleFalse_ThenCursorIsHidden) {
  Window window(64, 64, "Hide Cursor Test");

  window.setCursorVisible(false);

  EXPECT_FALSE(window.isCursorVisible());
}

TEST(Window, GivenWindow_WhenSetCursorVisibleTrue_ThenCursorIsVisible) {
  Window window(64, 64, "Show Cursor Test");

  window.setCursorVisible(false);
  window.setCursorVisible(true);

  EXPECT_TRUE(window.isCursorVisible());
}

TEST(Window, GivenVisibleCursor_WhenToggleCursor_ThenCursorBecomesHidden) {
  Window window(64, 64, "Toggle Cursor Test");

  EXPECT_TRUE(window.isCursorVisible());

  window.toggleCursor();

  EXPECT_FALSE(window.isCursorVisible());
}

TEST(Window, GivenHiddenCursor_WhenToggleCursor_ThenCursorBecomesVisible) {
  Window window(64, 64, "Toggle Cursor Test 2");

  window.setCursorVisible(false);
  EXPECT_FALSE(window.isCursorVisible());

  window.toggleCursor();

  EXPECT_TRUE(window.isCursorVisible());
}

// =============================================================================
// Window shouldClose Tests
// =============================================================================

TEST(Window, GivenNewWindow_WhenShouldClose_ThenReturnsFalse) {
  Window window(64, 64, "Should Close Test");

  EXPECT_FALSE(window.shouldClose());
}

// =============================================================================
// Window Resize Stability Tests
// =============================================================================

TEST(Window, GivenNewWindow_WhenGetLastResizeTimeNs_ThenReturnsZero) {
  Window window(64, 64, "Resize Time Test");

  EXPECT_EQ(window.getLastResizeTimeNs(), 0u);
}

TEST(Window, GivenNoResize_WhenIsResizeStable_ThenReturnsFalse) {
  Window window(64, 64, "Stable Resize Test");

  // No resize has occurred, so it should not be "stable"
  EXPECT_FALSE(window.isResizeStable(100));
}
