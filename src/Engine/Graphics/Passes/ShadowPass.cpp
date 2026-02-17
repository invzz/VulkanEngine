#include "Engine/Graphics/Passes/ShadowPass.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "Editor/RenderContext.hpp"
#include "glm/geometric.hpp"
// #include "glm/gtc/matrix_inverse.hpp"

namespace engine {

    void ShadowPass::execute(FrameInfo& frameInfo) {
        // Keep target-locked directional/spot lights oriented correctly for this frame.
        LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

        // Upload dynamic light arrays (SSBO) and reflect counts into the UBO.
        auto const lightCounts = engineState_->renderContext->updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);
        GlobalUbo  ubo{};
        ubo.pointLightCount       = lightCounts.point;
        ubo.directionalLightCount = lightCounts.directional;
        ubo.spotLightCount        = lightCounts.spot;

        // Render shadow maps for all shadow-casting lights
        engineState_->shadowSystem->renderShadowMaps(frameInfo, engineState_->shadowSettings);

        ubo.projection    = frameInfo.camera.getProjection();
        ubo.view          = frameInfo.camera.getView();
        ubo.invProjection = glm::inverse(ubo.projection);
        ubo.invView       = glm::inverse(ubo.view);
        if (frameInfo.scene->getRegistry().valid(frameInfo.cameraEntity) && frameInfo.scene->getRegistry().all_of<TransformComponent>(frameInfo.cameraEntity)) {
            ubo.cameraPosition = glm::vec4(frameInfo.scene->getRegistry().get<TransformComponent>(frameInfo.cameraEntity).translation, 1.0f);
        } else {
            ubo.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        ubo.shadowLightCount            = engineState_->shadowSystem->getShadowLightCount();
        ubo.directionalCascadeCount     = engineState_->shadowSystem->getDirectionalCascadeCount();
        ubo.directionalCascadeBaseIndex = engineState_->shadowSystem->getDirectionalCascadeBaseIndex();
        ubo.directionalCascadeSplits    = engineState_->shadowSystem->getDirectionalCascadeSplits();
        ubo.cascadeBlendWidth           = engineState_->shadowSettings.cascadeBlendWidth;

        // // Fog Logic (same as App::shadowPhase)
        // glm::vec3 horizonColor   = fogSettings_.color;
        // glm::vec3 zenithColor    = fogSettings_.color;
        // float     currentDensity = fogSettings_.density;

        // if (fogSettings_.useSkyColor)
        // {
        //   SunInfo const sunInfo         = queryPrimaryDirectionalLightSunInfo(*frameInfo.scene);
        //   float const   visualSunHeight = sunInfo.directionToSun.y;

        //   glm::vec3 const dayHorizon = glm::vec3(0.7f, 0.8f, 0.9f);
        //   glm::vec3 const dayZenith  = glm::vec3(0.2f, 0.4f, 0.8f);

        //   glm::vec3 const sunsetHorizon = glm::vec3(0.8f, 0.4f, 0.1f);
        //   glm::vec3 const sunsetZenith  = glm::vec3(0.2f, 0.2f, 0.4f);

        //   glm::vec3 const nightHorizon = glm::vec3(0.01f, 0.01f, 0.02f);
        //   glm::vec3 const nightZenith  = glm::vec3(0.0f, 0.0f, 0.005f);

        //   if (visualSunHeight > 0.2f)
        //   {
        //     horizonColor = dayHorizon;
        //     zenithColor  = dayZenith;
        //   }
        //   else if (visualSunHeight > -0.1f)
        //   {
        //     float const t = (visualSunHeight + 0.1f) / 0.3f;
        //     horizonColor  = glm::mix(sunsetHorizon, dayHorizon, t);
        //     zenithColor   = glm::mix(sunsetZenith, dayZenith, t);
        //   }
        //   else if (visualSunHeight > -0.3f)
        //   {
        //     float const t = (visualSunHeight + 0.3f) / 0.2f;
        //     horizonColor  = glm::mix(nightHorizon, sunsetHorizon, t);
        //     zenithColor   = glm::mix(nightZenith, sunsetZenith, t);

        //     currentDensity = fogSettings_.density * glm::mix(0.2f, 1.0f, t);
        //   }
        //   else
        //   {
        //     horizonColor   = nightHorizon;
        //     zenithColor    = nightZenith;
        //     currentDensity = fogSettings_.density * 0.2f;
        //   }
        // }

        // ubo.fogColor         = glm::vec4(horizonColor, currentDensity);
        // ubo.fogZenithColor   = glm::vec4(zenithColor, 0.0f);
        ubo.fogHeight        = engineState_->fogSettings.height;
        ubo.fogHeightDensity = engineState_->fogSettings.heightDensity;

        // HZB settings copied into UBO
        ubo.hzbMaxMipLevel     = engineState_->hzbSettings.maxMipLevel;
        ubo.hzbMinScreenPixels = engineState_->hzbSettings.minScreenPixels;
        ubo.hzbScreenSizeScale = engineState_->hzbSettings.screenSizeScale;
        ubo.hzbEnabled         = engineState_->hzbSettings.enabled;

        // Frustum planes
        glm::mat4 const vp   = ubo.projection * ubo.view;
        glm::mat4       vpT  = glm::transpose(vp);
        glm::vec4 const row0 = vpT[0];
        glm::vec4 const row1 = vpT[1];
        glm::vec4 const row2 = vpT[2];
        glm::vec4 const row3 = vpT[3];

        ubo.frustumPlanes[0] = row3 + row0;  // Left
        ubo.frustumPlanes[1] = row3 - row0;  // Right
        ubo.frustumPlanes[2] = row3 + row1;  // Bottom
        ubo.frustumPlanes[3] = row3 - row1;  // Top
        ubo.frustumPlanes[4] = row2;         // Near
        ubo.frustumPlanes[5] = row3 - row2;  // Far

        for (auto& frustumPlane : ubo.frustumPlanes) {
            float const len = glm::length(glm::vec3(frustumPlane));
            frustumPlane /= len;
        }

        for (int i = 0; i < ubo.shadowLightCount; i++) {
            ubo.lightSpaceMatrices[i] = engineState_->shadowSystem->getLightSpaceMatrix(i);
        }

        ubo.cubeShadowLightCount = engineState_->shadowSystem->getCubeShadowLightCount();
        for (int i = 0; i < ubo.cubeShadowLightCount && i < 4; i++) {
            ubo.pointLightShadowData[i] = glm::vec4(engineState_->shadowSystem->getPointLightPosition(i), engineState_->shadowSystem->getPointLightRange(i));
        }

        // Copy editor/debug selection into GPU UBO so shaders can react to debugMode changes.
        ubo.debugMode = frameInfo.debugMode;

        engineState_->renderContext->updateUBO(frameInfo.frameIndex, ubo);
    }

}  // namespace engine
