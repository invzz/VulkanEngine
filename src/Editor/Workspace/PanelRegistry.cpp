#include "Editor/Workspace/PanelRegistry.hpp"

namespace engine {

    PanelRegistry::PanelRegistry() = default;

    void PanelRegistry::registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
        DockConstraints constraints) {
        panels_[name]      = std::move(panel);
        constraints_[name] = constraints;
    }

    bool PanelRegistry::unregisterPanel(const std::string& name) {
        auto it = panels_.find(name);
        if (it != panels_.end()) {
            panels_.erase(it);
            constraints_.erase(name);
            return true;
        }
        return false;
    }

    UIPanel* PanelRegistry::getPanel(const std::string& name) {
        auto it = panels_.find(name);
        if (it != panels_.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    std::vector<std::string> PanelRegistry::getPanelNames() const {
        std::vector<std::string> names;
        names.reserve(panels_.size());
        for (const auto& [name, _] : panels_) {
            names.push_back(name);
        }
        return names;
    }

    DockConstraints PanelRegistry::getConstraints(const std::string& name) const {
        auto it = constraints_.find(name);
        if (it != constraints_.end()) {
            return it->second;
        }
        return DockConstraints{};
    }

    void PanelRegistry::setConstraints(const std::string& name, const DockConstraints& constraints) {
        constraints_[name] = constraints;
    }

    bool PanelRegistry::hasPanel(const std::string& name) const {
        return panels_.find(name) != panels_.end();
    }

    void PanelRegistry::showPanel(const std::string& name) {
        auto it = panels_.find(name);
        if (it != panels_.end()) {
            it->second->setVisible(true);
        }
    }

    void PanelRegistry::hidePanel(const std::string& name) {
        auto it = panels_.find(name);
        if (it != panels_.end()) {
            it->second->setVisible(false);
        }
    }

    void PanelRegistry::togglePanel(const std::string& name) {
        auto it = panels_.find(name);
        if (it != panels_.end()) {
            it->second->setVisible(!it->second->isVisible());
        }
    }

    std::vector<UIPanel*> PanelRegistry::getVisiblePanels() const {
        std::vector<UIPanel*> visible;
        for (const auto& [_, panel] : panels_) {
            if (panel->isVisible()) {
                visible.push_back(panel.get());
            }
        }
        return visible;
    }

    std::vector<UIPanel*> PanelRegistry::getAllPanels() const {
        std::vector<UIPanel*> all;
        all.reserve(panels_.size());
        for (const auto& [_, panel] : panels_) {
            all.push_back(panel.get());
        }
        return all;
    }

}  // namespace engine
