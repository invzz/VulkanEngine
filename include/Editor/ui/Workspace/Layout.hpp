#ifndef EDITOR_WORKSPACE_LAYOUT_HPP
#define EDITOR_WORKSPACE_LAYOUT_HPP
#include <string>
namespace engine {
    /**
 * @brief Docking zone positions for panels.
 *
 * Defines the canonical zones a panel can occupy in the editor layout.
 * These are used by WorkspaceManager to enforce layout rules.
 */
    enum class DockZone {
        None,
        Top,
        Bottom,
        Left,
        Right,
        Center,
        DockTop,
        DockBottom,
        DockLeft,
        DockRight,
        DockCenter,
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
        std::string dockToPanel;
    };
    /**
 * @brief Layout preset for the editor.
 *
 * Presets define a complete layout configuration that can be applied at once.
 */
    enum class LayoutPreset {
        Default,
        Minimal,
        Modeling,
        Rendering,
        Custom,
    };
}  // namespace engine
#endif
