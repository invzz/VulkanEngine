#include "Engine/Graphics/Passes/CompositionPass.hpp"

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

namespace {
    struct SunInfo {
        glm::vec3 directionToSun{0.0f, 1.0f, 0.0f};
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float     intensity{0.0f};
        bool      valid{false};
    };

    SunInfo queryPrimaryDirectionalLightSunInfo(engine::Scene const& scene) {
        SunInfo info{};

        auto const& registry = scene.getRegistry();
        auto        view     = registry.view<engine::TransformComponent, engine::DirectionalLightComponent>();
        for (auto entity : view) {
            auto const& transform = view.get<engine::TransformComponent>(entity);
            auto const& light     = view.get<engine::DirectionalLightComponent>(entity);

            glm::vec3 const lightRayDir = glm::normalize(transform.getForwardDir());
            info.directionToSun         = -lightRayDir;
            info.color                  = light.color;
            info.intensity              = light.intensity;
            info.valid                  = true;
            break;
        }

        return info;
    }
}  // namespace

namespace engine {

    void CompositionPass::execute(FrameInfo& frameInfo) {
        // Update post-process descriptors
        auto imageInfo = renderer_.getOffscreenImageInfo(frameInfo.frameIndex);
        auto depthInfo = renderer_.getDepthImageInfo(frameInfo.frameIndex);

        // Refresh the post-process descriptor set each frame (image/depth views may change on resize)
        DescriptorWriter(descriptorAccess_.getPostProcessSetLayout(), descriptorAccess_.getDescriptorPool()).writeImage(0, &imageInfo).writeImage(1, &depthInfo).overwrite(descriptorAccess_.postProcessDescriptorSetRef(frameInfo.frameIndex));

        auto& postProcessPush = runtimeState_.postProcessPushRef();
        postProcessPush.inverseProjection = glm::inverse(camera_.getProjection());
        postProcessPush.projection        = camera_.getProjection();

        postProcessPush.debugMode = frameInfo.debugMode;

        // Begin swapchain render pass for composition + UI (swapchain render pass is not active elsewhere)
        renderer_.beginSwapChainRenderPass(frameInfo.commandBuffer);

        rendering_.postProcessingSystem->render(frameInfo, descriptorAccess_.postProcessDescriptorSetRef(frameInfo.frameIndex), postProcessPush);
        if (compositionPort_ != nullptr) {
            compositionPort_->renderUI(frameInfo, frameInfo.commandBuffer, window_.isCursorVisible());
        }

        // End swapchain render pass started above
        renderer_.endSwapChainRenderPass(frameInfo.commandBuffer);
    }

}  // namespace engine
