#include "Editor/ui/Panels/CameraPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
namespace engine {
    CameraPanel::CameraPanel(EngineState& state)
        : state_(state) {}
    void CameraPanel::render(FrameInfo& frameInfo) {
        entt::entity camEntity = state_.cameraEntity();
        Scene&       scene     = state_.scene();
        if (camEntity == entt::null || !scene.getRegistry().valid(camEntity)) {
            ui::UI::TextDisabled("No active camera");
            return;
        }
        ImGui::Text("Camera Control");
        ui::UI::Separator();
        auto& tc = scene.getRegistry().get<TransformComponent>(camEntity);
        ui::UI::DragFloat3("Position", &tc.translation.x, 0.1f);
        ui::UI::DragFloat3("Rotation", &tc.rotation.x, 0.5f);
    }
}  // namespace engine
