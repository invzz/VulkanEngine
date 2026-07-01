#include "Editor/ui/Panels/InspectorPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/Panels/AnimationPanel.hpp"
#include "Editor/ui/Panels/LightsPanel.hpp"
#include "Editor/ui/Panels/TransformPanel.hpp"
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
        if (!visible_) {
            return;
        }

        ui::UI::PushThemeStyle();

        if (ImGui::Begin("Inspector", &visible_)) {
            if (frameInfo.selectedEntity != entt::null) {
                auto& registry = frameInfo.scene->getRegistry();
                if (!registry.valid(frameInfo.selectedEntity)) {
                    frameInfo.selectedEntity = entt::null;
                } else {
                    ui::UI::BeginSurface("inspector_transform");
                    ui::UI::SectionTitle("Transform", "Position, rotation, scale");
                    transformPanel_->render(frameInfo);
                    ui::UI::EndSurface();

                    ui::UI::BeginSurface("inspector_lights");
                    ui::UI::SectionTitle("Lights", "Shading and shadow parameters");
                    lightsPanel_->render(frameInfo);
                    ui::UI::EndSurface();

                    ui::UI::BeginSurface("inspector_animation");
                    ui::UI::SectionTitle("Animation", "Playback and clip state");
                    animationPanel_->render(frameInfo);
                    ui::UI::EndSurface();
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
