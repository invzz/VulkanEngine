#include "Engine/Graphics/Passes/ShadowPass.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "glm/geometric.hpp"
// #include "glm/gtc/matrix_inverse.hpp"

namespace engine {

    void ShadowPass::execute(FrameInfo& frameInfo) {
        // Keep target-locked directional/spot lights oriented correctly for this frame.
        LightSystem::updateAllTargetLockedLights(*frameInfo.scene);

        // Upload dynamic light arrays (SSBO) and reflect counts into the UBO.
        auto const lightCounts = renderContextPort_->updateLightBuffers(frameInfo.frameIndex, *frameInfo.scene);
        GlobalUbo     ubo{};
        GlobalUboCold uboCold{};
        ubo.pointLightCount       = lightCounts.point;
        ubo.directionalLightCount = lightCounts.directional;
        ubo.spotLightCount        = lightCounts.spot;

        // Render shadow maps for all shadow-casting lights
        rendering_.shadowSystem->renderShadowMaps(frameInfo, *sceneRuntime_.shadowSettings);

        ubo.projection    = frameInfo.camera.getProjection();
        ubo.view          = frameInfo.camera.getView();
        ubo.invProjection = glm::inverse(ubo.projection);
        ubo.invView       = glm::inverse(ubo.view);
        if (sceneRuntime_.scene->getRegistry().valid(frameInfo.cameraEntity) && sceneRuntime_.scene->getRegistry().all_of<TransformComponent>(frameInfo.cameraEntity)) {
            ubo.cameraPosition = glm::vec4(sceneRuntime_.scene->getRegistry().get<TransformComponent>(frameInfo.cameraEntity).translation, 1.0f);
        } else {
            ubo.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        ubo.shadowLightCount = rendering_.shadowSystem->getShadowLightCount();

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
            ubo.lightSpaceMatrices[i] = rendering_.shadowSystem->getLightSpaceMatrix(i);
        }

        ubo.cubeShadowLightCount = rendering_.shadowSystem->getCubeShadowLightCount();
        for (int i = 0; i < ubo.cubeShadowLightCount && i < 4; i++) {
            ubo.pointLightShadowData[i] = glm::vec4(rendering_.shadowSystem->getPointLightPosition(i), rendering_.shadowSystem->getPointLightRange(i));
        }

        // Copy editor/debug selection into GPU UBO so shaders can react to debugMode changes.
        ubo.debugMode = frameInfo.debugMode;

        renderContextPort_->updateUBO(frameInfo.frameIndex, ubo, uboCold);
    }

}  // namespace engine
