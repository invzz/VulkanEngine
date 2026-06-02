#pragma once

#include "Engine/Application/Ports/IDescriptorAccessPort.hpp"
#include "Engine/Application/Ports/IRuntimeStatePort.hpp"
#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;
class UIManager;
class Camera;
class Scene;
class Window;

class CompositionPass : public IRenderPass {
 public:
  CompositionPass(Renderer& renderer, RenderingStateView rendering, IDescriptorAccessPort& descriptorAccess, IRuntimeStatePort& runtimeState, UIManager* uiManager, Camera& camera, Window& window)
      : renderer_(renderer), rendering_(rendering), descriptorAccess_(descriptorAccess), runtimeState_(runtimeState), uiManager_(uiManager), camera_(camera), window_(window) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "Composition";
    return name;
  }

 private:
  Renderer& renderer_;
  RenderingStateView rendering_;
  IDescriptorAccessPort& descriptorAccess_;
  IRuntimeStatePort& runtimeState_;
  UIManager* uiManager_ = nullptr;
  Camera& camera_;
  Window& window_;
};

}  // namespace engine
