#ifndef EDITOR_WORKSPACE_SELECTION_SYSTEM_HPP
#define EDITOR_WORKSPACE_SELECTION_SYSTEM_HPP

#include <entt/entt.hpp>
#include <functional>
#include <set>

namespace engine {

    /**
 * @brief Manages entity selection in the editor.
 *
 * Supports single and multi-selection, selection change notifications,
 * and selection filtering by component type.
 */
    class SelectionSystem {
       public:
        SelectionSystem();
        ~SelectionSystem() = default;

        /**
     * @brief Set the selection mode.
     */
        enum class Mode { Single,
            Multi };
        void setMode(Mode mode) {
            mode_ = mode;
        }
        Mode getMode() const {
            return mode_;
        }

        /**
     * @brief Clear all selections.
     */
        void clear();

        /**
     * @brief Select an entity (replaces selection in single mode).
     * @return true if the selection changed.
     */
        bool select(entt::entity entity);

        /**
     * @brief Add an entity to the selection (multi mode only).
     * @return true if the entity was added.
     */
        bool addSelection(entt::entity entity);

        /**
     * @brief Remove an entity from the selection.
     * @return true if the entity was removed.
     */
        bool removeSelection(entt::entity entity);

        /**
     * @brief Toggle an entity's selection state.
     * @return true if the selection changed.
     */
        bool toggleSelection(entt::entity entity);

        /**
     * @brief Get the current selection.
     */
        const std::set<entt::entity>& getSelection() const {
            return selection_;
        }

        /**
     * @brief Get the active (primary) selection.
     * In single mode, this is the only selection.
     * In multi mode, this is the most recently selected entity.
     */
        entt::entity getActiveSelection() const {
            return activeSelection_;
        }

        /**
     * @brief Check if an entity is selected.
     */
        bool isSelected(entt::entity entity) const;

        /**
     * @brief Check if the selection is empty.
     */
        bool isEmpty() const {
            return selection_.empty();
        }

        /**
     * @brief Get the number of selected entities.
     */
        size_t getSelectionCount() const {
            return selection_.size();
        }

        /**
     * @brief Register a callback for selection changes.
     */
        void onSelectionChanged(std::function<void(entt::entity, bool)> callback);

        /**
     * @brief Filter selection to entities with a specific component.
     */
        template <typename Component>
        void filterByComponent(entt::registry& registry) {
            std::set<entt::entity> newSelection;
            for (auto entity : selection_) {
                if (registry.all_of<Component>(entity)) {
                    newSelection.insert(entity);
                }
            }
            if (newSelection != selection_) {
                selection_       = newSelection;
                activeSelection_ = selection_.empty() ? entt::null : *selection_.rbegin();
                if (callback_) {
                    for (auto entity : selection_) {
                        callback_(entity, true);
                    }
                }
            }
        }

       private:
        std::set<entt::entity>                  selection_;
        entt::entity                            activeSelection_ = entt::null;
        Mode                                    mode_            = Mode::Single;
        std::function<void(entt::entity, bool)> callback_;
    };

}  // namespace engine

#endif
