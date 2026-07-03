#ifndef EDITOR_WORKSPACE_SHORTCUT_MANAGER_HPP
#define EDITOR_WORKSPACE_SHORTCUT_MANAGER_HPP

#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace engine {

    /**
 * @brief Manages keyboard shortcuts for the editor.
 *
 * Allows registering callbacks for key combinations. Shortcuts are
 * checked during the input processing phase.
 */
    class ShortcutManager {
       public:
        ShortcutManager();
        ~ShortcutManager() = default;

        /**
     * @brief Register a shortcut.
     * @param name Unique name for the shortcut.
     * @param key GLFW key code.
     * @param modifiers Bitmask of GLFW modifier keys (GLFW_MOD_CONTROL, etc.).
     * @param callback Callback to invoke when the shortcut is triggered.
     */
        void registerShortcut(const std::string& name, int key, int modifiers,
            std::function<void()> callback);

        /**
     * @brief Unregister a shortcut by name.
     */
        void unregisterShortcut(const std::string& name);

        /**
     * @brief Check all registered shortcuts against current input state.
     * @param glfwWindow GLFW window handle (for glfwGetKey/glfwGetModKeyState).
     * @return true if any shortcut was triggered.
     */
        bool checkShortcuts(GLFWwindow* glfwWindow);

        /**
     * @brief Check if a specific shortcut is registered.
     */
        bool hasShortcut(const std::string& name) const;

        /**
     * @brief Get all registered shortcut names.
     */
        std::vector<std::string> getShortcutNames() const;

       private:
        struct ShortcutEntry {
            int                   key;
            int                   modifiers;
            std::function<void()> callback;
        };

        std::unordered_map<std::string, ShortcutEntry> shortcuts_;
    };

}  // namespace engine

#endif
