#include "Editor/ui/ViewportPanel.hpp"

#include <algorithm>
#include <cstdint>
#include <imgui.h>

#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Viewport.hpp"

namespace engine {

    ViewportPanel::ViewportPanel()
        : UIPanel(),
          extent_({400, 300}) {}

    void ViewportPanel::setViewport(Viewport* viewport, VkExtent2D extent) {
        viewport_ = viewport;
        extent_   = extent;
    }

    void ViewportPanel::setWindow(Window* window) {
        window_ = window;
    }

    void ViewportPanel::setMouse(Mouse* mouse) {
        mouse_ = mouse;
    }

    void ViewportPanel::render(FrameInfo& frameInfo) {
        updateModeFromUI(frameInfo);
        applyCursorState(frameInfo.viewportMode);

        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

        // Resize MUST happen inside the window so GetContentRegionAvail()
        // returns the actual viewport panel content area — calling it outside
        // a window returns (0,0) and the offscreen FB is never resized,
        // permanently stuck at the initial 400×300 extent.
        handleResize();

        if (viewport_) {
            ImVec2 contentAvail = ImGui::GetContentRegionAvail();
            if (contentAvail.x > 0 && contentAvail.y > 0) {
                ImTextureID texID = viewport_->getImTextureID(frameInfo.frameIndex);
                if (texID) {
                    ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
                    ImGui::Image(texID, contentAvail, ImVec2(0, 0), ImVec2(1, 1));

                    handlePickingInput(frameInfo);
                }
            }
        }

        if (frameInfo.viewportMode == ViewportMode::Navigation) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "NAVIGATING — Press ESC to exit");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "PICKING — Click objects to select, Right-click to navigate");
        }

        ImGui::End();
    }

    void ViewportPanel::enterNavigation(FrameInfo& frameInfo) {
        frameInfo.viewportMode = ViewportMode::Navigation;
        if (mouse_) {
            mouse_->reset();
        }
    }

    void ViewportPanel::exitNavigation(FrameInfo& frameInfo) {
        frameInfo.viewportMode = ViewportMode::Picking;
        if (mouse_) {
            mouse_->reset();
        }
    }

    void ViewportPanel::updateModeFromUI(FrameInfo& frameInfo) {
        // ESC exits navigation regardless of which panel is focused.
        if (frameInfo.viewportMode == ViewportMode::Navigation && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            exitNavigation(frameInfo);
        }
    }

    void ViewportPanel::applyCursorState(ViewportMode mode) {
        if (!window_)
            return;

        const bool wantsNavigation = (mode == ViewportMode::Navigation);
        if (window_->isCursorNavigationMode() != wantsNavigation) {
            if (mouse_) {
                mouse_->reset();
            }
            window_->setCursorMode(wantsNavigation);
        }
    }

    void ViewportPanel::handlePickingInput(FrameInfo& frameInfo) {
        // Always clear the click flag unless this frame actually starts a picking click.
        frameInfo.viewportMouseClicked = false;

        if (frameInfo.viewportMode != ViewportMode::Picking)
            return;

        if (!ImGui::IsItemHovered())
            return;

        // Right-click enters navigation immediately (Blender/Fly-style).
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            enterNavigation(frameInfo);
            return;
        }

        // Left-click in picking mode is consumed for object selection.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mouseScreen = ImGui::GetMousePos();
            ImVec2 itemMin     = ImGui::GetItemRectMin();
            ImVec2 itemMax     = ImGui::GetItemRectMax();
            ImVec2 itemSize    = ImVec2(itemMax.x - itemMin.x, itemMax.y - itemMin.y);

            ImVec2 local(mouseScreen.x - itemMin.x, mouseScreen.y - itemMin.y);
            if (itemSize.x > 0.0f && itemSize.y > 0.0f) {
                frameInfo.viewportMousePos.x   = local.x / itemSize.x;
                frameInfo.viewportMousePos.y   = local.y / itemSize.y;
                frameInfo.viewportMouseClicked = true;
            }
        }
    }

    void ViewportPanel::handleResize() {
        // Use the content region (not the window outer size). The ImGui::Image
        // is drawn at GetContentRegionAvail() at the start of the panel, so
        // the offscreen framebuffer MUST match that exact area — otherwise
        // the texture gets stretched/squished to fit.
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        if (contentAvail.x < 1.0f || contentAvail.y < 1.0f) {
            return;  // panel is collapsed; nothing to do
        }

        // CRITICAL (HiDPI / fractional scaling): ImGui works in *logical*
        // points, but the swapchain and offscreen framebuffer are sized in
        // *physical* pixels. On a scaled display (e.g. Wayland fractional
        // scaling) the physical pixels differ from logical points, so a
        // content area of 628x843 logical points actually covers
        // 628*scale x 843*scale physical pixels on screen. If we size the FB
        // at the logical size, the scene is rendered at logical resolution
        // and then upscaled by ImGui to the physical area → blurry / soft /
        // "low resolution" even when the panel is held still.
        //
        // Determine the scale from the ground truth (GLFW framebuffer/window
        // ratio) when a Window is available; fall back to ImGui's
        // DisplayFramebufferScale otherwise. This makes the offscreen FB
        // render at native physical resolution.
        const ImGuiIO& io     = ImGui::GetIO();
        float          scaleX = (io.DisplayFramebufferScale.x > 0.0f) ? io.DisplayFramebufferScale.x : 1.0f;
        float          scaleY = (io.DisplayFramebufferScale.y > 0.0f) ? io.DisplayFramebufferScale.y : 1.0f;
        if (window_ != nullptr) {
            if (GLFWwindow* glfwWin = window_->getGLFWwindow()) {
                int winW = 0, winH = 0, fbW = 0, fbH = 0;
                glfwGetWindowSize(glfwWin, &winW, &winH);
                glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
                if (winW > 0 && winH > 0 && fbW > 0 && fbH > 0) {
                    scaleX = static_cast<float>(fbW) / static_cast<float>(winW);
                    scaleY = static_cast<float>(fbH) / static_cast<float>(winH);
                }
            }
        }

        // Floor + clamp to a sane minimum so a 0×0 panel never asks the GPU
        // for a 0-sized image.
        uint32_t newW = std::max(1u, static_cast<uint32_t>(contentAvail.x * scaleX));
        uint32_t newH = std::max(1u, static_cast<uint32_t>(contentAvail.y * scaleY));
        // 1px epsilon: ImGui's GetContentRegionAvail() can return sub-pixel
        // wobble while the user holds the panel still (e.g. due to rounding
        // after DPI changes). Avoids full FB recreations on no-op frames.
        constexpr uint32_t kResizeEpsilon = 1;
        if (std::abs(static_cast<int32_t>(newW) - static_cast<int32_t>(extent_.width)) <= static_cast<int32_t>(kResizeEpsilon) &&
            std::abs(static_cast<int32_t>(newH) - static_cast<int32_t>(extent_.height)) <= static_cast<int32_t>(kResizeEpsilon)) {
            return;
        }
        extent_ = {newW, newH};
        if (onResize) {
            onResize(extent_);
        }
    }

}  // namespace engine
