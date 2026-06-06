#ifndef EDITOR_WORKSPACE_LAYOUT_HPP
#define EDITOR_WORKSPACE_LAYOUT_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "entt/entity/fwd.hpp"

namespace engine {

    /**
 * @brief Docking zone positions for panels.
 *
 * Defines the canonical zones a panel can occupy in the editor layout.
 * These are used by WorkspaceManager to enforce layout rules.
 */
    enum class DockZone {
        None,
        Top,         // Above viewport (toolbar, etc.)
        Bottom,      // Below viewport
        Left,        // Left of viewport
        Right,       // Right of viewport
        Center,      // Overlapping viewport (floating/overlay)
        DockTop,     // Docked to top of dockspace
        DockBottom,  // Docked to bottom of dockspace
        DockLeft,    // Docked to left of dockspace
        DockRight,   // Docked to right of dockspace
        DockCenter,  // Docked to center of dockspace (default)
    };

    /**
 * @brief Panel docking constraints.
 *
 * Defines rules for how a panel can be docked and where it can go.
 */
    struct DockConstraints {
        DockZone    preferredZone = DockZone::DockCenter;
        bool        resizable     = true;
        bool        closable      = true;
        bool        dockable      = true;
        bool        floatable     = true;
        float       minSizeX      = 200.0f;
        float       minSizeY      = 150.0f;
        std::string dockToPanel;  // Optional: prefer docking next to another panel
    };

    /**
 * @brief Layout preset for the editor.
 *
 * Presets define a complete layout configuration that can be applied at once.
 */
    enum class LayoutPreset {
        Default,    // Scene left, Inspector right, toolbar top
        Minimal,    // Maximize viewport, hide panels
        Modeling,   // Scene left, Inspector right, Viewport center
        Rendering,  // Viewport center, Inspector right, Settings bottom
        Custom,     // User-defined layout
    };

}  // namespace engine

#endif  // EDITOR_WORKSPACE_LAYOUT_HPP
