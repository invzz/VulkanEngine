#ifndef EDITOR_WORKSPACE_LAYOUT_BUILDER_HPP
#define EDITOR_WORKSPACE_LAYOUT_BUILDER_HPP
#include <imgui.h>

#include <string>
#include <vector>

#include "Layout.hpp"
namespace engine {
    /**
     * @brief One panel's intended placement in a layout preset.
     *
     * Maps a panel name (registry key) to a docking zone. The LayoutBuilder
     * reads these entries and turns them into ImGui::DockBuilder* calls.
     */
    struct LayoutEntry {
        std::string panelName;
        DockZone    zone;
    };
    /**
     * @brief Builds editor layouts using ImGui::DockBuilder.
     *
     * Why this exists: ImGui only auto-docks if a dockspace has been pre-split
     * with the DockBuilder API. Calling ImGui::DockSpace() without first calling
     * DockBuilder* leaves every panel as a floating top-level window.
     *
     * Usage:
     *   LayoutBuilder builder;
     *   builder.addEntry("Scene Objects", DockZone::DockLeft);
     *   builder.addEntry("Inspector",      DockZone::DockRight);
     *   builder.addEntry("Viewport",       DockZone::DockCenter);
     *   builder.apply(dockspaceId, ImVec2{w, h}, LayoutPreset::Default);
     *
     * `apply()` is idempotent and safe to call multiple times; the caller is
     * responsible for gating it (once per preset change, or "Reset Layout").
     */
    class LayoutBuilder {
       public:
        LayoutBuilder() = default;
        void   addEntry(const std::string& panelName, DockZone zone);
        void   clear();
        void   apply(ImGuiID dockspaceId, const ImVec2& dockspaceSize, LayoutPreset preset);
        size_t entryCount() const {
            return entries_.size();
        }

       private:
        void                     dockWindowToNode(ImGuiID nodeId, const std::string& windowName);
        std::vector<LayoutEntry> entries_;
    };
}  // namespace engine
#endif
