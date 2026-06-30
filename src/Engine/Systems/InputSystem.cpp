#include "Engine/Systems/InputSystem.hpp"

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"

namespace engine {

    InputSystem::InputSystem(Keyboard& keyboard, Mouse& mouse, Window& window) : keyboard_{keyboard}, mouse_{mouse}, window_{window} {}

    void InputSystem::update(FrameInfo& frameInfo) {
        // In picking mode the cursor is normal: don't consume camera movement.
        if (frameInfo.viewportMode == ViewportMode::Picking) {
            return;
        }

        // Control the selected object (camera or a game object)
        entt::entity const controllableEntity = frameInfo.selectedEntity != entt::null ? frameInfo.selectedEntity : frameInfo.cameraEntity;

        if (frameInfo.scene->getRegistry().valid(controllableEntity)) {
            auto& transform = frameInfo.scene->getRegistry().get<TransformComponent>(controllableEntity);
            keyboard_.moveInPlaneXZ(frameInfo.frameTime, transform);
            mouse_.lookAround(frameInfo.frameTime, transform);
        }
    }

}  // namespace engine
