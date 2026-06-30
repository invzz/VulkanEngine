#include "Editor/ui/ViewportPanel.hpp"

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
        handleResize();
        updateModeFromUI(frameInfo);
        applyCursorState(frameInfo.viewportMode);

        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

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
                frameInfo.viewportMousePos.x     = local.x / itemSize.x;
                frameInfo.viewportMousePos.y     = local.y / itemSize.y;
                frameInfo.viewportMouseClicked     = true;
            }
        }
    }

    void ViewportPanel::handleResize() {
        ImVec2 winSize = ImGui::GetWindowSize();
        if (winSize.x > 0 && winSize.y > 0) {
            VkExtent2D newExtent = {static_cast<uint32_t>(winSize.x), static_cast<uint32_t>(winSize.y)};
            if (newExtent.width != extent_.width || newExtent.height != extent_.height) {
                extent_ = newExtent;
                if (onResize) {
                    onResize(newExtent);
                }
            }
        }
    }

}  // namespace engine
