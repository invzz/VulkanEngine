#include "Engine/Graphics/IRenderContextPort.hpp"

#include "Editor/RenderContext.hpp"

namespace engine {

    IRenderContextPort::LightCounts RenderContextAdapter::updateLightBuffers(int frameIndex, Scene& scene) {
        auto [point, directional, spot] = ctx_->updateLightBuffers(frameIndex, scene);
        return IRenderContextPort::LightCounts{point, directional, spot};
    }

    void RenderContextAdapter::updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) {
        ctx_->updateUBO(frameIndex, ubo, uboCold);
    }

    VkDescriptorSet RenderContextAdapter::getGlobalDescriptorSet(int frameIndex) {
        return ctx_->getGlobalDescriptorSet(frameIndex);
    }

    VkDescriptorSetLayout RenderContextAdapter::getGlobalSetLayout() {
        return ctx_->getGlobalSetLayout();
    }

}  // namespace engine
