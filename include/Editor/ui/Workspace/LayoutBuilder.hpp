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
        std::string panelName;  // registry key, e.g. "Scene Objects"
        DockZone    zone;       // preferred zone
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

        /// Add a panel → zone mapping. Order matters only for ambiguous zones
        /// (e.g. two panels both targeting Center will be tabbed in insertion order).
        void addEntry(const std::string& panelName, DockZone zone);

        /// Remove all entries. Call before re-adding if changing the preset.
        void clear();

        /// Build the layout into `dockspaceId` (size = work-area size).
        /// Calls ImGui::DockBuilderRemoveNode, AddNode, SplitNode, DockWindow, Finish.
        /// `dockZone` panels become tabs inside the same node when they share a zone.
        void apply(ImGuiID dockspaceId, const ImVec2& dockspaceSize, LayoutPreset preset);

        /// Number of registered entries.
        size_t entryCount() const {
            return entries_.size();
        }

       private:
        // Internal: dock a single window name into a node id, respecting DockConstraints
        // (min size is applied as ImGuiWindowClass for now; full enforcement happens
        // in WorkspaceManager::enforceConstraints if needed).
        void dockWindowToNode(ImGuiID nodeId, const std::string& windowName);

        std::vector<LayoutEntry> entries_;
    };

}  // namespace engine

#endif  // EDITOR_WORKSPACE_LAYOUT_BUILDER_HPP
