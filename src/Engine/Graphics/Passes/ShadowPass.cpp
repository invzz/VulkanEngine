#include "Engine/Graphics/Passes/ShadowPass.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneUtils.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "glm/geometric.hpp"

namespace engine {

    ShadowPass::ShadowPass(ShadowSystem& shadow, IRenderContextPort& renderCtx,
        Scene& scene, ShadowSettings& shadowSettings)
        : RenderPassBase("Shadow"), shadow_(shadow), renderCtx_(renderCtx), scene_(scene), shadowSettings_(shadowSettings) {}

    void ShadowPass::execute(FrameInfo& frameInfo) {
        LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

        auto const lightCounts = renderCtx_.updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);

        GlobalUbo ubo{};
        ubo.pointLightCount       = lightCounts.point;
        ubo.directionalLightCount = lightCounts.directional;
        ubo.spotLightCount        = lightCounts.spot;

        ubo.projection    = frameInfo.camera.getProjection();
        ubo.view          = frameInfo.camera.getView();
        ubo.invProjection = glm::inverse(ubo.projection);
        ubo.invView       = glm::inverse(ubo.view);

        ubo.cameraPosition = getCameraPosition(scene_, frameInfo.cameraEntity);

        // --- Cascaded shadow maps for directional lights ---
        // Find the first directional light's direction from the scene
        glm::vec3 lightDir(0.0f, -1.0f, 0.0f);  // fallback: straight down
        bool hasDirectionalLight = false;
        {
            auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : dirView) {
                auto& transform = dirView.get<TransformComponent>(entity);
                lightDir         = transform.getForwardDir();
                hasDirectionalLight = true;
                break;  // only the first directional light gets cascades
            }
        }

        if (hasDirectionalLight) {
            shadow_.computeCascades(ubo.view, ubo.projection, lightDir);
        }

        shadow_.renderShadowMaps(frameInfo, shadowSettings_);

        // Fill cascade data into UBO
        ubo.cascadeCount = shadow_.getCascadeCount();
        for (int i = 0; i < ubo.cascadeCount && i < ShadowSystem::MAX_CASCADES; i++) {
            auto const& cd          = shadow_.getCascadeData(i);
            ubo.cascadeLightMatrices[i] = cd.lightSpaceMatrix;
            ubo.cascadeSplits[i]        = cd.splitDepth;
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

}  // namespace engine
