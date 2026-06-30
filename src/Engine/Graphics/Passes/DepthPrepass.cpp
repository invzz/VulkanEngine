#include "Engine/Graphics/Passes/DepthPrepass.hpp"

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

namespace engine {

    DepthPrepass::DepthPrepass(ModelRenderSystem& models, Renderer& renderer)
        : models_(models), renderer_(renderer) {}

    void DepthPrepass::execute(FrameInfo& frameInfo) {
        renderer_.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
        models_.renderDepthPrepass(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }

    const std::string& DepthPrepass::getName() const {
        static std::string n = "DepthPrepass";
        return n;
    }

}  // namespace engine
