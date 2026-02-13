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
  // Handle cursor toggle (ESC key with debouncing)
  bool const toggleKeyPressed = keyboard_.isKeyPressed(keyboard_.mappings.toggleCursor);
  if (toggleKeyPressed && !lastToggleKeyState_) {
    // When toggling cursor visibility, also update Mouse state so we don't
    // get a large accumulated delta when entering FPS (cursor-hidden) mode.
    bool wasVisible = window_.isCursorVisible();
    window_.toggleCursor();
    // Reset mouse internal state so we don't pick up a large delta on the
    // first frame after toggling visibility. We avoid calling private
    // lock/unlock helpers from here and just reset the initialized flag.
    mouse_.reset();
  }
  lastToggleKeyState_ = toggleKeyPressed;

  // Only process movement/look input when in FPS mode (cursor hidden)
  // When cursor is visible, user should only interact with UI
  if (window_.isCursorVisible()) {
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
