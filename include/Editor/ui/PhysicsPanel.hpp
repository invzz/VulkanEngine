#ifndef EDITOR_PHYSICSPANEL_HPP
#define EDITOR_PHYSICSPANEL_HPP

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

/**
 * @brief UI panel for managing physics components on entities
 */
class PhysicsPanel : public UIPanel {
public:
    PhysicsPanel(Scene& scene, bool* simulationRunning);

    void render(FrameInfo& frameInfo) override;

    [[nodiscard]] bool isSeparateWindow() const override {
        return true;
    }

private:
    Scene& scene_;
    bool* simulationRunning_ = nullptr;

    /**
     * @brief Add physics component to selected entity
     */
    void addPhysicsComponent(FrameInfo& frameInfo);

    /**
     * @brief Edit physics properties of selected entity
     */
    void editPhysicsProperties(FrameInfo& frameInfo);
};

} // namespace engine

#endif // EDITOR_PHYSICSPANEL_HPP