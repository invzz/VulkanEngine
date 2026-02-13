#ifndef FORWARD_OVERLAY_PASS_HPP
#define FORWARD_OVERLAY_PASS_HPP
#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"
namespace engine {
class ForwardOverlayPass : public RenderPass {
 public:
  ForwardOverlayPass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "ForwardOverlay";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    renderer.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);

    systems.skybox().render(frameInfo);
    systems.model().renderTransmission(frameInfo);
    systems.model().renderAlphaBlend(frameInfo);

    renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
}  // namespace engine
#endif  // FORWARD_OVERLAY_PASS_HPP