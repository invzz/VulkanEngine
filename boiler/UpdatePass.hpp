#ifndef UPDATE_PASS_HPP
#define UPDATE_PASS_HPP

#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"
namespace engine {
class UpdatePass : public IRenderPass {
 public:
  UpdatePass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "Update";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    systems.objectSelection().update(frameInfo);
    systems.input().update(frameInfo);
    systems.lod().update(frameInfo);
    systems.camera().update(frameInfo, renderer.getAspectRatio());
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
}  // namespace engine

#endif  // UPDATE_PASS_HPP