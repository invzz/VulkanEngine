#include "Editor/ui/InspectorPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/AnimationPanel.hpp"
#include "Editor/ui/LightsPanel.hpp"
#include "Editor/ui/TransformPanel.hpp"
#include "Editor/ui/UI.hpp"
#include "entt/entity/entity.hpp"

namespace engine {

    InspectorPanel::InspectorPanel(EngineState& state)
        : state_(state) {
        transformPanel_ = std::make_unique<TransformPanel>(state.scene());
        lightsPanel_    = std::make_unique<LightsPanel>(state.scene());
        animationPanel_ = std::make_unique<AnimationPanel>(state.scene());
    }

    void InspectorPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        ui::UI::PushThemeStyle();

        if (ImGui::Begin("Inspector", &visible_)) {
            if (frameInfo.selectedEntity != entt::null) {
                auto& registry = frameInfo.scene->getRegistry();
                if (!registry.valid(frameInfo.selectedEntity)) {
                    frameInfo.selectedEntity = entt::null;
                } else {
                    ui::UI::TextColored("Transform", ImVec4(0.6f, 0.85f, 1.0f, 1.0f));
                    ui::UI::Separator();
                    transformPanel_->render(frameInfo);
                    ui::UI::Separator();

                    ui::UI::TextColored("Lights", ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
                    ui::UI::Separator();
                    lightsPanel_->render(frameInfo);
                    ui::UI::Separator();

                    ui::UI::TextColored("Animation", ImVec4(0.5f, 1.0f, 0.6f, 1.0f));
                    ui::UI::Separator();
                    animationPanel_->render(frameInfo);
                }
            } else {
                ui::UI::TextDisabled("No entity selected.");
                ui::UI::TextDisabled("Select an entity in the Scene panel to inspect it.");
            }
        }
        ImGui::End();

        ui::UI::PopThemeStyle();
    }

}  // namespace engine
