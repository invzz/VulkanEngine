#include "Editor/Infrastructure/RenderContextAdapter.hpp"

#include "Editor/RenderContext.hpp"

namespace engine {

RenderContextAdapter::RenderContextAdapter(RenderContext* renderContext)
    : renderContext_(renderContext) {}

IRenderContextPort::LightCounts RenderContextAdapter::updateLightBuffers(int frameIndex, Scene& scene) {
  auto counts = renderContext_->updateLightBuffers(frameIndex, scene);
  return IRenderContextPort::LightCounts{counts.point, counts.directional, counts.spot};
}

void RenderContextAdapter::updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) {
  renderContext_->updateUBO(frameIndex, ubo, uboCold);
}

VkDescriptorSet RenderContextAdapter::getGlobalDescriptorSet(int frameIndex) {
  return renderContext_->getGlobalDescriptorSet(frameIndex);
}

VkDescriptorSetLayout RenderContextAdapter::getGlobalSetLayout() {
  return renderContext_->getGlobalSetLayout();
}

}  // namespace engine
