#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"

using namespace engine;

TEST(Window, GivenValidParameters_WhenConstructed_ThenWindowIsCreated) {
    Window window(64, 64, "Construction Test");

    EXPECT_TRUE(window.getGLFWwindow() != nullptr);
}

TEST(Window, GivenWindow_WhenGetGLFWwindow_ThenReturnsValidHandle) {
    Window window(64, 64, "GLFW Handle Test");

    GLFWwindow* glfwWindow = window.getGLFWwindow();
    EXPECT_NE(glfwWindow, nullptr);
}

TEST(Window, GivenWindow_WhenResetWindowResizedFlag_ThenFlagIsFalse) {
    Window window(64, 64, "Reset Resize Test");

    window.resetWindowResizedFlag();

    EXPECT_FALSE(window.wasWindowResized());
}

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

TEST(Window, GivenNewWindow_WhenShouldClose_ThenReturnsFalse) {
    Window window(64, 64, "Should Close Test");

    EXPECT_FALSE(window.shouldClose());
}

TEST(Window, GivenNoResize_WhenIsResizeStable_ThenReturnsFalse) {
    Window window(64, 64, "Stable Resize Test");

    EXPECT_FALSE(window.isResizeStable(100));
}
