#include "Editor/ui/Panels/ViewportPanel.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>

#include <ImGuizmo.h>

#include <algorithm>
#include <cstdint>

#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Viewport.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"

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

        // ImGuizmo requires BeginFrame once per frame, before any panel
        // windows, to initialize its internal state (creates a transparent
        // full-viewport window that is immediately closed).
        ImGuizmo::BeginFrame();

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

                    bool viewportToolbarHovered = renderViewportToolbar(frameInfo, imageTopLeft);
                    renderFpsBadge(frameInfo, imageTopLeft, contentAvail);

                    // Render gizmo overlay if a valid entity is selected, then
                    // pick — the gizmo gate is handled inside handlePickingInput.
                    handleGizmo(frameInfo, imageTopLeft, contentAvail);
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

        ImGui::End();
    }

    bool ViewportPanel::renderViewportToolbar(FrameInfo& frameInfo, const ImVec2& topLeft) {
        ImGui::SetCursorScreenPos(ImVec2(topLeft.x + 10.0f, topLeft.y + 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.12f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.45f, 0.55f, 0.45f));

        ImGuiWindowFlags toolbarFlags =
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::BeginChild("ViewportTools", ImVec2(310.0f, 36.0f), ImGuiChildFlags_Borders, toolbarFlags);

        auto showTooltip = [](const char* text) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)) {
                ImGui::SetTooltip("%s", text);
            }
        };

        auto gOp          = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        bool gizmoEnabled = frameInfo.gizmoEnabled;

        auto gOpButton = [&](const char* icon, int mode, bool active, const char* suffix, const char* tooltip) {
            bool clicked = ui::UI::ToolbarIcon(icon, active, suffix);
            if (clicked && gizmoEnabled) {
                frameInfo.gizmoOperation = (gOp == mode) ? ImGuizmo::TRANSLATE : mode;
            }
            showTooltip(tooltip);
        };

        gOpButton(ICON_FA_UP_DOWN_LEFT_RIGHT, ImGuizmo::TRANSLATE,
            (gOp & ImGuizmo::TRANSLATE) == ImGuizmo::TRANSLATE, "vp_translate", "Translate (T)");
        ImGui::SameLine(0.0f, 3.0f);
        gOpButton(ICON_FA_ROTATE, ImGuizmo::ROTATE,
            (gOp & ImGuizmo::ROTATE) == ImGuizmo::ROTATE, "vp_rotate", "Rotate (R)");
        ImGui::SameLine(0.0f, 3.0f);
        gOpButton(ICON_FA_ARROWS_UP_DOWN, ImGuizmo::SCALE,
            (gOp & ImGuizmo::SCALE) == ImGuizmo::SCALE, "vp_scale", "Scale (S)");

        ImGui::SameLine(0.0f, 6.0f);
        bool isWorld = (frameInfo.gizmoMode == 1);
        if (ui::UI::ToolbarIcon(ICON_FA_GLOBE, isWorld, "vp_space")) {
            if (gizmoEnabled) {
                frameInfo.gizmoMode = isWorld ? 0 : 1;
            }
        }
        showTooltip("Toggle world/local space (W)");

        ImGui::SameLine(0.0f, 3.0f);
        if (ui::UI::ToolbarIcon(ICON_FA_CROSSHAIRS, frameInfo.gizmoEnabled, "vp_gizmo_toggle")) {
            frameInfo.gizmoEnabled = !frameInfo.gizmoEnabled;
        }
        showTooltip("Enable/disable gizmo");

        ImGui::SameLine(0.0f, 6.0f);
        bool nav = (frameInfo.viewportMode == ViewportMode::Navigation);
        if (ui::UI::ToolbarIcon(ICON_FA_COMPASS, nav, "vp_mode")) {
            frameInfo.viewportMode = nav ? ViewportMode::Picking : ViewportMode::Navigation;
        }
        showTooltip(nav ? "Viewport mode: Navigation" : "Viewport mode: Picking");

        bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return hovered;
    }

    void ViewportPanel::renderFpsBadge(const FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size) const {
        float fps     = (frameInfo.frameTime > 0.0f) ? (1.0f / frameInfo.frameTime) : 0.0f;
        float frameMs = frameInfo.frameTime * 1000.0f;

        ImVec4 windowBg  = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImVec4 border    = ui::UI::GetBorderColor();
        ImVec4 textColor = ui::UI::GetTextColor();

        char perfText[96];
        std::snprintf(
            perfText,
            sizeof(perfText),
            "%s %.1f ms   %s %.0f FPS",
            ICON_FA_STOPWATCH,
            frameMs,
            ICON_FA_GAUGE_HIGH,
            fps);

        ImVec2          textSize           = ImGui::CalcTextSize(perfText);
        constexpr float kToolbarChipHeight = 36.0f;  // Matches viewport gizmo toolbar child height.
        constexpr float kToolbarChipWidth  = 180.0f;
        ImVec2          badgeSize(kToolbarChipWidth, kToolbarChipHeight);
        ImVec2          badgeMin(topLeft.x + size.x - badgeSize.x - 10.0f, topLeft.y + 10.0f);
        ImVec2          badgeMax(badgeMin.x + badgeSize.x, badgeMin.y + badgeSize.y);

        ImVec4 badgeBg     = ImVec4(windowBg.x, windowBg.y, windowBg.z, 0.92f);
        ImVec4 badgeBorder = ImVec4(border.x, border.y, border.z, 0.85f);

        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(badgeBg), 8.0f);
        fg->AddRect(badgeMin, badgeMax, ImGui::GetColorU32(badgeBorder), 8.0f);
        float textY = badgeMin.y + ((badgeSize.y - textSize.y) * 0.5f);
        fg->AddText(ImVec2(badgeMin.x + 8.0f, textY), ImGui::GetColorU32(textColor), perfText);
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

        // Don't pick when the gizmo is being used or hovered — the click is for the gizmo.
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) {
            return;
        }

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

    void ViewportPanel::handleGizmo(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size) {
        // No valid entity selected → nothing to manipulate.
        if (frameInfo.selectedEntity == entt::null || !frameInfo.gizmoEnabled)
            return;

        auto& registry = frameInfo.scene->getRegistry();
        if (!registry.valid(frameInfo.selectedEntity))
            return;
        if (!registry.all_of<TransformComponent>(frameInfo.selectedEntity))
            return;

        auto& transform = registry.get<TransformComponent>(frameInfo.selectedEntity);

        // Set up ImGuizmo rect in logical (ImGui) coordinates.
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(topLeft.x, topLeft.y, size.x, size.y);

        // Convert camera matrices from our Vulkan Z=0..1 convention to the
        // OpenGL Z=-1..1 convention that ImGuizmo expects internally.
        // Additionally, our Camera uses up = {0, -1, 0} (inverted Y in view
        // space), so we must also negate the projection Y scale to compensate
        // — otherwise the gizmo Y axis appears flipped on screen.
        const glm::mat4& view = frameInfo.camera.getView();
        const glm::mat4& proj = frameInfo.camera.getProjection();

        glm::mat4 projGL(1.0f);
        projGL[2][2]           = 2.0f;   // scale: [0,1] → [0,2]
        projGL[3][2]           = -1.0f;  // bias:  [0,2] → [-1,1]
        glm::mat4 projForGizmo = projGL * proj;
        projForGizmo[1][1] *= -1.0f;  // flip Y: camera.up = -Y

        // Get the object's world transform as a mutable float[16] array.
        // ImGuizmo::Manipulate modifies this in-place when the user drags.
        glm::mat4 objectMatrix = transform.modelTransform();
        float     matrix[16];
        memcpy(matrix, glm::value_ptr(objectMatrix), sizeof(matrix));

        auto operation = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        auto mode      = static_cast<ImGuizmo::MODE>(frameInfo.gizmoMode);

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projForGizmo),
            operation,
            mode,
            matrix);

        // If the user manipulated the gizmo, decompose the new matrix back
        // to translation / rotation / scale and write to the component.
        if (ImGuizmo::IsUsing()) {
            glm::mat4 newMatrix;
            memcpy(&newMatrix, matrix, sizeof(matrix));

            // Translation is stored in column 3 (last column).
            transform.translation = glm::vec3(newMatrix[3]);

            // Scale = length of each column's 3-component vector (before rotation).
            glm::vec3 newScale(
                glm::length(glm::vec3(newMatrix[0])),
                glm::length(glm::vec3(newMatrix[1])),
                glm::length(glm::vec3(newMatrix[2])));

            // Remove scale to get a pure rotation matrix, then extract Euler
            // angles matching TransformComponent's YXZ (yaw-pitch-roll) order.
            glm::mat4 R = newMatrix;
            if (newScale.x > 1e-6f)
                R[0] /= newScale.x;
            if (newScale.y > 1e-6f)
                R[1] /= newScale.y;
            if (newScale.z > 1e-6f)
                R[2] /= newScale.z;

            // modelTransform() uses Ry * Rx * Rz with Euler angles Y (yaw),
            // X (pitch), Z (roll).  From the column-major rotation matrix:
            //   R[2] = (c2*s1,  -s2,  c1*c2)
            //   R[0] = (...,     c2*s3, ...)
            //   R[1] = (...,     c2*c3, ...)
            const float s2 = -R[2][1];                   // sin(x)
            const float c2 = std::sqrt(1.0f - s2 * s2);  // cos(x), could be negated

            if (std::abs(c2) > 1e-6f) {
                // No gimbal lock.
                transform.rotation.x = std::asin(s2);
                transform.rotation.y = std::atan2(R[2][0], R[2][2]);
                transform.rotation.z = std::atan2(R[0][1], R[1][1]);
            } else {
                // Gimbal lock: cos(x) ~ 0, pitch is ±90°.
                // Only yaw+roll matter; set roll to 0 and extract yaw from R[0].
                transform.rotation.x = (s2 > 0.0f) ? glm::half_pi<float>() : -glm::half_pi<float>();
                transform.rotation.y = std::atan2(-R[0][2], R[0][0]);
                transform.rotation.z = 0.0f;
            }

            transform.scale = newScale;
        }
    }

}  // namespace engine
