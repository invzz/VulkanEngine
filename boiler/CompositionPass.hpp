#ifndef COMPOSITION_PASS_HPP
#define COMPOSITION_PASS_HPP

#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"
namespace engine {
class CompositionPass : public RenderPass {
 public:
  CompositionPass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "Composition";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    renderer.beginSwapChainRenderPass(frameInfo.commandBuffer);

    systems.postProcessing().render(frameInfo);

    systems.ui().render(frameInfo, frameInfo.commandBuffer);

    renderer.endSwapChainRenderPass(frameInfo.commandBuffer);
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
}  // namespace engine