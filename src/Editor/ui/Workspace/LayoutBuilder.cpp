#include "Editor/ui/Workspace/LayoutBuilder.hpp"

#include <imgui.h>

#include <algorithm>
#include <imgui_internal.h>  // DockBuilder API

namespace engine {

    void LayoutBuilder::addEntry(const std::string& panelName, DockZone zone) {
        entries_.push_back({panelName, zone});
    }

    void LayoutBuilder::clear() {
        entries_.clear();
    }

    void LayoutBuilder::apply(ImGuiID dockspaceId, const ImVec2& dockspaceSize, LayoutPreset preset) {
        if (dockspaceId == 0 || entries_.empty()) {
            return;
        }

        // 1) Wipe any existing dock tree.
        ImGui::DockBuilderRemoveNode(dockspaceId);
        // 2) Create a fresh node covering the dockspace rect.
        ImGuiID root = ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(root, dockspaceSize);

        // Default layout (also used for Modeling / Rendering — they all share the
        // 4-zone skeleton; preset-specific tweaks can fork here later).
        ImGuiID leftId   = 0;
        ImGuiID rightId  = 0;
        ImGuiID bottomId = 0;
        ImGuiID centerId = root;  // remaining area after splits

        switch (preset) {
            case LayoutPreset::Default:
            case LayoutPreset::Modeling:
            case LayoutPreset::Rendering: {
                // Left sidebar: 20% width.
                leftId = ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.20f, nullptr, &centerId);
                // Right sidebar: 25% width of what's left.
                rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.25f, nullptr, &centerId);
                // Bottom: 25% height of what's left.
                bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.28f, nullptr, &centerId);
                break;
            }
            case LayoutPreset::Minimal: {
                // No splits — everything tabbed in the center.
                break;
            }
            case LayoutPreset::Custom: {
                // Custom means: no automatic splits, just dock everything into root
                // and let the user rearrange.
                break;
            }
        }

        // 3) Dock every entry into the node for its zone. Multiple panels in the
        // same zone will share the node as tabs (this is the default DockBuilder
        // behavior when you dock several windows to the same node id).
        for (const auto& e : entries_) {
            ImGuiID target     = root;
            bool    shouldDock = true;
            switch (e.zone) {
                case DockZone::DockLeft:
                    target = leftId;
                    break;
                case DockZone::DockRight:
                    target = rightId;
                    break;
                case DockZone::DockBottom:
                    target = bottomId;
                    break;
                case DockZone::DockCenter:
                    target = centerId;
                    break;
                case DockZone::DockTop:
                    target = root;
                    break;  // top-level overlay
                case DockZone::None:
                case DockZone::Top:
                case DockZone::Bottom:
                case DockZone::Left:
                case DockZone::Right:
                case DockZone::Center:
                    shouldDock = false;
                    break;  // keep floating / manually positioned
                default:
                    target = centerId;
                    break;
            }
            if (shouldDock && target != 0) {
                dockWindowToNode(target, e.panelName);
            }
        }

        // 4) Commit the builder. After this call, DockSpace() will see the layout.
        ImGui::DockBuilderFinish(dockspaceId);
    }

    void LayoutBuilder::dockWindowToNode(ImGuiID nodeId, const std::string& windowName) {
        if (nodeId == 0 || windowName.empty()) {
            return;
        }
        ImGui::DockBuilderDockWindow(windowName.c_str(), nodeId);
    }

}  // namespace engine
