#ifndef EDITOR_PHYSICSPANEL_HPP
#define EDITOR_PHYSICSPANEL_HPP

#include "Engine/Scene/Scene.hpp"

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class JoltPhysicsSystem;

    /**
 * @brief UI panel for managing physics components on entities
 */
    class PhysicsPanel : public UIPanel {
       public:
        PhysicsPanel(Scene& scene, bool* simulationRunning, bool* showColliderWireframes, bool* solidGroundEnabled, JoltPhysicsSystem* joltPhysicsSystem);

        void render(FrameInfo& frameInfo) override;

        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }

       private:
        Scene&             scene_;
        bool*              simulationRunning_      = nullptr;
        bool*              showColliderWireframes_ = nullptr;
        bool*              solidGroundEnabled_     = nullptr;
        JoltPhysicsSystem* joltPhysicsSystem_      = nullptr;

        /**
     * @brief Add physics component to selected entity
     */
        void addPhysicsComponent(FrameInfo& frameInfo);

        /**
     * @brief Edit physics properties of selected entity
     */
        void editPhysicsProperties(FrameInfo& frameInfo);

        /**
     * @brief Edit collider properties of selected entity
     */
        void editColliderProperties(FrameInfo& frameInfo);
    };

}  // namespace engine

#endif  // EDITOR_PHYSICSPANEL_HPP