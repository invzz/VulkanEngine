#ifndef EDITOR_WORKSPACE_PANEL_REGISTRY_HPP
#define EDITOR_WORKSPACE_PANEL_REGISTRY_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Editor/ui/UIPanel.hpp"
#include "Layout.hpp"

namespace engine {

    /**
 * @brief Registry for editor panels with lifecycle management.
 *
 * Manages panel registration, visibility, and docking. Panels are
 * identified by string keys and can be queried by type or name.
 */
    class PanelRegistry {
       public:
        PanelRegistry();
        ~PanelRegistry() = default;

        /**
     * @brief Register a panel with the registry.
     * @param name Unique name for the panel.
     * @param panel Panel instance (takes ownership).
     * @param constraints Docking constraints for this panel.
     */
        void registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
            DockConstraints constraints = DockConstraints{});

        /**
     * @brief Unregister a panel by name.
     * @return true if the panel was found and removed.
     */
        bool unregisterPanel(const std::string& name);

        /**
     * @brief Get a panel by name.
     * @return Pointer to the panel, or nullptr if not found.
     */
        UIPanel* getPanel(const std::string& name);

        /**
     * @brief Get a panel by type (returns nullptr if not found).
     */
        template <typename T>
        T* getPanel() {
            for (auto& [name, panel] : panels_) {
                if (auto* typed = dynamic_cast<T*>(panel.get())) {
                    return typed;
                }
            }
            return nullptr;
        }

        /**
     * @brief Get all registered panel names.
     */
        std::vector<std::string> getPanelNames() const;

        /**
     * @brief Get the docking constraints for a panel.
     */
        DockConstraints getConstraints(const std::string& name) const;

        /**
     * @brief Set the docking constraints for a panel.
     */
        void setConstraints(const std::string& name, const DockConstraints& constraints);

        /**
     * @brief Check if a panel is registered.
     */
        bool hasPanel(const std::string& name) const;

        /**
     * @brief Make a panel visible.
     */
        void showPanel(const std::string& name);

        /**
     * @brief Hide a panel.
     */
        void hidePanel(const std::string& name);

        /**
     * @brief Toggle panel visibility.
     */
        void togglePanel(const std::string& name);

        /**
     * @brief Get all visible panels.
     */
        std::vector<UIPanel*> getVisiblePanels() const;

        /**
     * @brief Get all panels (regardless of visibility).
     */
        std::vector<UIPanel*> getAllPanels() const;

       private:
        std::unordered_map<std::string, std::unique_ptr<UIPanel>> panels_;
        std::unordered_map<std::string, DockConstraints>          constraints_;
    };

}  // namespace engine

#endif  // EDITOR_WORKSPACE_PANEL_REGISTRY_HPP
