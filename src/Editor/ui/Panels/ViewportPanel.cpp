#include "Editor/ui/Panels/ViewportPanel.hpp"

#include <imgui.h>

#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <entt/entt.hpp>

#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Viewport.hpp"

#include "Editor/ui/Panels/ViewportObjectGizmo.hpp"
#include "Editor/ui/Panels/ViewportToolbar.hpp"
#include "Editor/ui/Panels/ViewportViewGizmo.hpp"
#include "ImViewGuizmo.h"
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
        ImGuizmo::BeginFrame();
        ImViewGuizmo::BeginFrame();
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);
        handleResize();
        if (viewport_ != nullptr) {
            ImVec2 contentAvail = ImGui::GetContentRegionAvail();
            if (contentAvail.x > 0 && contentAvail.y > 0) {
                ImTextureID texID = viewport_->getImTextureID(frameInfo.frameIndex);
                if (texID != 0u) {
                    ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
                    ImGui::Image(texID, contentAvail, ImVec2(0, 0), ImVec2(1, 1));
                    // Capture the viewport image's rect + hover state NOW, before the
                    // toolbar/gizmo submit their own ImGui items and overwrite the
                    // "last item" state that IsItemHovered()/GetItemRect*() read.
                    imageRectMin_               = ImGui::GetItemRectMin();
                    imageRectMax_               = ImGui::GetItemRectMax();
                    imageHovered_               = ImGui::IsItemHovered();
                    bool viewportToolbarHovered = ViewportToolbar::render(frameInfo, imageTopLeft, contentAvail);
                    ViewportViewGizmo::render(frameInfo, imageTopLeft, contentAvail);
                    ViewportObjectGizmo::render(frameInfo, imageTopLeft, contentAvail);
                    if (!viewportToolbarHovered) {
                        handlePickingInput(frameInfo);
                    } else {
                        frameInfo.viewportMouseClicked = false;
                    }
                }
            }
        }
        if (frameInfo.viewportMode == ViewportMode::Navigation) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "NAVIGATING — Press ESC to exit");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "PICKING — Click objects to select, Right-click to navigate");
        }
        ImGui::SameLine();
        ImGui::TextColored(
            ImVec4(0.55f, 0.8f, 1.0f, 1.0f),
            " | View Gizmo: %s",
            frameInfo.viewGizmoOrbitSelected ? "Orbit Selected" : "Look In Place");
        ImGui::End();
    }
    void ViewportPanel::enterNavigation(FrameInfo& frameInfo) {
        frameInfo.viewportMode = ViewportMode::Navigation;
        if (mouse_ != nullptr) {
            mouse_->reset();
        }
    }
    void ViewportPanel::exitNavigation(FrameInfo& frameInfo) {
        frameInfo.viewportMode = ViewportMode::Picking;
        if (mouse_ != nullptr) {
            mouse_->reset();
        }
    }
    void ViewportPanel::updateModeFromUI(FrameInfo& frameInfo) {
        if (frameInfo.viewportMode == ViewportMode::Navigation && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            exitNavigation(frameInfo);
        }
    }
    void ViewportPanel::applyCursorState(ViewportMode mode) {
        if (window_ == nullptr) {
            return;
        }
        const bool wantsNavigation = (mode == ViewportMode::Navigation);
        if (window_->isCursorNavigationMode() != wantsNavigation) {
            if (mouse_ != nullptr) {
                mouse_->reset();
            }
            window_->setCursorMode(wantsNavigation);
        }
    }
    void ViewportPanel::handlePickingInput(FrameInfo& frameInfo) {
        frameInfo.viewportMouseClicked = false;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver() || ImViewGuizmo::IsUsing() || ImViewGuizmo::IsOver()) {
            return;
        }
        if (frameInfo.viewportMode != ViewportMode::Picking) {
            return;
        }
        if (!imageHovered_) {
            return;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            enterNavigation(frameInfo);
            return;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mouseScreen = ImGui::GetMousePos();
            ImVec2 itemMin     = imageRectMin_;
            ImVec2 itemMax     = imageRectMax_;
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
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        if (contentAvail.x < 1.0f || contentAvail.y < 1.0f) {
            return;
        }
        const ImGuiIO& io     = ImGui::GetIO();
        float          scaleX = (io.DisplayFramebufferScale.x > 0.0f) ? io.DisplayFramebufferScale.x : 1.0f;
        float          scaleY = (io.DisplayFramebufferScale.y > 0.0f) ? io.DisplayFramebufferScale.y : 1.0f;
        if (window_ != nullptr) {
            if (GLFWwindow* glfwWin = window_->getGLFWwindow()) {
                int winW = 0;
                int winH = 0;
                int fbW  = 0;
                int fbH  = 0;
                glfwGetWindowSize(glfwWin, &winW, &winH);
                glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
                if (winW > 0 && winH > 0 && fbW > 0 && fbH > 0) {
                    scaleX = static_cast<float>(fbW) / static_cast<float>(winW);
                    scaleY = static_cast<float>(fbH) / static_cast<float>(winH);
                }
            }
        }
        uint32_t           newW           = std::max(1u, static_cast<uint32_t>(contentAvail.x * scaleX));
        uint32_t           newH           = std::max(1u, static_cast<uint32_t>(contentAvail.y * scaleY));
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
