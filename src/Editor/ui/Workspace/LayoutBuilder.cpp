#include "Editor/ui/Workspace/LayoutBuilder.hpp"

#include <imgui.h>

#include <algorithm>
#include <imgui_internal.h>
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
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGuiID root = ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(root, dockspaceSize);
        ImGuiID leftId   = 0;
        ImGuiID rightId  = 0;
        ImGuiID bottomId = 0;
        ImGuiID centerId = root;
        switch (preset) {
            case LayoutPreset::Default:
            case LayoutPreset::Modeling:
            case LayoutPreset::Rendering: {
                leftId   = ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.20f, nullptr, &centerId);
                rightId  = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.25f, nullptr, &centerId);
                bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.28f, nullptr, &centerId);
                break;
            }
            case LayoutPreset::Minimal: {
                break;
            }
            case LayoutPreset::Custom: {
                break;
            }
        }
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
                    break;
                case DockZone::None:
                case DockZone::Top:
                case DockZone::Bottom:
                case DockZone::Left:
                case DockZone::Right:
                case DockZone::Center:
                    shouldDock = false;
                    break;
                default:
                    target = centerId;
                    break;
            }
            if (shouldDock && target != 0) {
                dockWindowToNode(target, e.panelName);
            }
        }
        ImGui::DockBuilderFinish(dockspaceId);
    }
    void LayoutBuilder::dockWindowToNode(ImGuiID nodeId, const std::string& windowName) {
        if (nodeId == 0 || windowName.empty()) {
            return;
        }
        ImGui::DockBuilderDockWindow(windowName.c_str(), nodeId);
    }
}  // namespace engine
