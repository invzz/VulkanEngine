#ifndef DEFERRED_LIGHT_PASS_HPP
#define DEFERRED_LIGHT_PASS_HPP
#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"
namespace engine {
class DeferredLightingPass : public RenderPass {
 public:
  DeferredLightingPass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "DeferredLighting";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    renderer.beginDeferredLightingRenderPass(frameInfo.commandBuffer);

    systems.deferredLighting().render(frameInfo);

    renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
}  // namespace engine

#endif  // DEFERRED_LIGHT_PASS_HPP
