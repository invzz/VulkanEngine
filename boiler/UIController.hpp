#ifndef UI_CONTROLLER_HPP
#define UI_CONTROLLER_HPP

#include "Editor/ui/UIManager.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {
class UIController {
 public:
  UIController(Window&, Device&, Renderer&, Scene&);

  void render(FrameInfo&, VkCommandBuffer);

 private:
  std::unique_ptr<ImGuiManager> imgui;
  std::unique_ptr<UIManager> uiManager;
};
}  // namespace engine

#endif  // UI_CONTROLLER_HPP