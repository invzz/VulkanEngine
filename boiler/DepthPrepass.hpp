#ifndef DEPTH_PREPASS_HPP
#define DEPTH_PREPASS_HPP

#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"

namespace engine {
class DepthPrepass : public RenderPass {
 public:
  DepthPrepass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "DepthPrepass";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    renderer.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);

    systems.model().renderDepthPrepass(frameInfo);

    renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
}  // namespace engine

#endif  // DEPTH_PREPASS_HPP
