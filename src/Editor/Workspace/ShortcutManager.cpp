#include "Editor/Workspace/ShortcutManager.hpp"

namespace engine {

    ShortcutManager::ShortcutManager() = default;

    void ShortcutManager::registerShortcut(const std::string& name, int key, int modifiers,
        std::function<void()> callback) {
        shortcuts_[name] = {key, modifiers, std::move(callback)};
    }

    void ShortcutManager::unregisterShortcut(const std::string& name) {
        shortcuts_.erase(name);
    }

    bool ShortcutManager::checkShortcuts(GLFWwindow* glfwWindow) {
        bool triggered = false;
        for (auto& [name, entry] : shortcuts_) {
            if (glfwGetKey(glfwWindow, entry.key) == GLFW_PRESS) {
                // Check modifiers individually
                bool ctrl  = glfwGetKey(glfwWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                             glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
                bool shift = glfwGetKey(glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                             glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                bool alt   = glfwGetKey(glfwWindow, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                             glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
                bool super = glfwGetKey(glfwWindow, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
                             glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;

                int currentMods = 0;
                if (ctrl)
                    currentMods |= GLFW_MOD_CONTROL;
                if (shift)
                    currentMods |= GLFW_MOD_SHIFT;
                if (alt)
                    currentMods |= GLFW_MOD_ALT;
                if (super)
                    currentMods |= GLFW_MOD_SUPER;

                if ((currentMods & entry.modifiers) == entry.modifiers) {
                    entry.callback();
                    triggered = true;
                    break;  // Only trigger one shortcut per frame
                }
            }
        }
        return triggered;
    }

    bool ShortcutManager::hasShortcut(const std::string& name) const {
        return shortcuts_.find(name) != shortcuts_.end();
    }

    std::vector<std::string> ShortcutManager::getShortcutNames() const {
        std::vector<std::string> names;
        names.reserve(shortcuts_.size());
        for (const auto& [name, _] : shortcuts_) {
            names.push_back(name);
        }
        return names;
    }

}  // namespace engine
