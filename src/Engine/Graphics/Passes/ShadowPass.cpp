#include "Engine/Graphics/Passes/ShadowPass.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "glm/geometric.hpp"

namespace engine {

    ShadowPass::ShadowPass(ShadowSystem& shadow, IRenderContextPort& renderCtx,
        Scene& scene, ShadowSettings& shadowSettings)
        : shadow_(shadow), renderCtx_(renderCtx), scene_(scene), shadowSettings_(shadowSettings) {}

    void ShadowPass::execute(FrameInfo& frameInfo) {
        LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

        auto const lightCounts = renderCtx_.updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);

        GlobalUbo ubo{};
        ubo.pointLightCount       = lightCounts.point;
        ubo.directionalLightCount = lightCounts.directional;
        ubo.spotLightCount        = lightCounts.spot;

        shadow_.renderShadowMaps(frameInfo, shadowSettings_);

        ubo.projection    = frameInfo.camera.getProjection();
        ubo.view          = frameInfo.camera.getView();
        ubo.invProjection = glm::inverse(ubo.projection);
        ubo.invView       = glm::inverse(ubo.view);

        if (scene_.getRegistry().valid(frameInfo.cameraEntity) && scene_.getRegistry().all_of<TransformComponent>(frameInfo.cameraEntity)) {
            ubo.cameraPosition = glm::vec4(
                scene_.getRegistry().get<TransformComponent>(frameInfo.cameraEntity).translation, 1.0f);
        } else {
            ubo.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        ubo.shadowLightCount = shadow_.getShadowLightCount();

        glm::mat4 const vp   = ubo.projection * ubo.view;
        glm::mat4       vpT  = glm::transpose(vp);
        ubo.frustumPlanes[0] = vpT[3] + vpT[0];
        ubo.frustumPlanes[1] = vpT[3] - vpT[0];
        ubo.frustumPlanes[2] = vpT[3] + vpT[1];
        ubo.frustumPlanes[3] = vpT[3] - vpT[1];
        ubo.frustumPlanes[4] = vpT[2];
        ubo.frustumPlanes[5] = vpT[3] - vpT[2];
        for (auto& p : ubo.frustumPlanes)
            p /= glm::length(glm::vec3(p));

        for (int i = 0; i < ubo.shadowLightCount; i++)
            ubo.lightSpaceMatrices[i] = shadow_.getLightSpaceMatrix(i);

        ubo.cubeShadowLightCount = shadow_.getCubeShadowLightCount();
        for (int i = 0; i < ubo.cubeShadowLightCount && i < 4; i++)
            ubo.pointLightShadowData[i] = glm::vec4(
                shadow_.getPointLightPosition(i), shadow_.getPointLightRange(i));

        ubo.debugMode = frameInfo.debugMode;

        GlobalUboCold uboCold{};
        renderCtx_.updateUBO(frameInfo.frameIndex, ubo, uboCold);
    }

    const std::string& ShadowPass::getName() const {
        static std::string n = "Shadow";
        return n;
    }

}  // namespace engine
