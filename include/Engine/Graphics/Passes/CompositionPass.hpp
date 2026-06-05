#pragma once

#include "Engine/Application/Ports/ICompositionPort.hpp"
#include "Engine/Application/Ports/IDescriptorAccessPort.hpp"
#include "Engine/Application/Ports/IRuntimeStatePort.hpp"
#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;
class Camera;
class Scene;
class Window;

class CompositionPass : public IRenderPass {
 public:
  CompositionPass(Renderer& renderer, RenderingStateView rendering, IDescriptorAccessPort& descriptorAccess, IRuntimeStatePort& runtimeState, ICompositionPort* compositionPort, Camera& camera, Window& window, bool drawUI = true)
      : renderer_(renderer), rendering_(rendering), descriptorAccess_(descriptorAccess), runtimeState_(runtimeState), compositionPort_(compositionPort), camera_(camera), window_(window), drawUI_(drawUI) {}

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
  ICompositionPort* compositionPort_ = nullptr;
  Camera& camera_;
  Window& window_;
  bool drawUI_ = true;
};

}  // namespace engine
