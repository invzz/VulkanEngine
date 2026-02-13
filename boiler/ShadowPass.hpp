#ifndef SHADOW_PASS_HPP
#define SHADOW_PASS_HPP

#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"

namespace engine {
class ShadowPass : public RenderPass {
 public:
  ShadowPass(SystemRegistry& systems, RenderContext& renderContext) : systems(systems), renderContext(renderContext) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "Shadow";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    GlobalUbo ubo{};

    LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

    auto counts = renderContext.updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);

    ubo.pointLightCount = counts.point;
    ubo.directionalLightCount = counts.directional;
    ubo.spotLightCount = counts.spot;

    systems.shadow().renderShadowMaps(frameInfo, systems.shadowSettings());

    ubo.projection = frameInfo.camera.getProjection();
    ubo.view = frameInfo.camera.getView();

    renderContext.updateUBO(frameInfo.frameIndex, ubo);
  }

 private:
  SystemRegistry& systems;
  RenderContext& renderContext;
};
}  // namespace engine
#endif  // SHADOW_PASS_HPP