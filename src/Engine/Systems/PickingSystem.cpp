#include "Engine/Systems/PickingSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <entt/entt.hpp>
#include <float.h>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    bool PickingSystem::intersectRayAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& tNear) const {
        glm::vec3 tMin   = (aabbMin - rayOrigin) / rayDir;
        glm::vec3 tMax   = (aabbMax - rayOrigin) / rayDir;
        glm::vec3 t1     = glm::min(tMin, tMax);
        glm::vec3 t2     = glm::max(tMin, tMax);
        glm::vec2 tNear2 = glm::min(glm::vec2(t1.x, t1.y), glm::vec2(t1.z, t1.x));  // Wait, standard slab method:
        // Let's just do it manually to avoid GLM version issues.
        float t1x = glm::min(tMin.x, tMax.x);
        float t1y = glm::min(tMin.y, tMax.y);
        float t1z = glm::min(tMin.z, tMax.z);
        float t2x = glm::max(tMin.x, tMax.x);
        float t2y = glm::max(tMin.y, tMax.y);
        float t2z = glm::max(tMin.z, tMax.z);

        float tEntry = glm::max(glm::max(t1x, t1y), t1z);
        float tExit  = glm::min(glm::min(t2x, t2y), t2z);

        if (tEntry > tExit || tExit < 0.0f) {
            return false;
        }

        if (tEntry > 0.0f) {
            tNear = tEntry;
        } else {
            tNear = 0.0f;
        }

        return true;
    }

    glm::vec3 PickingSystem::unprojectToWorldRay(float ndcX, float ndcY, glm::mat4 invProj, glm::mat4 invView) const {
        glm::vec4 clipNear(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 clipFar(ndcX, ndcY, 1.0f, 1.0f);

        glm::vec4 eyeNear = invProj * clipNear;
        glm::vec4 eyeFar  = invProj * clipFar;
        eyeNear /= eyeNear.w;
        eyeFar /= eyeFar.w;

        glm::vec3 worldNear = glm::vec3(invView * eyeNear);
        glm::vec3 worldFar  = glm::vec3(invView * eyeFar);

        return glm::normalize(worldFar - worldNear);
    }

    std::optional<entt::entity> PickingSystem::pickViewport(FrameInfo& frameInfo, float viewportMouseX, float viewportMouseY) {
        float ndcX = (viewportMouseX * 2.0f) - 1.0f;
        float ndcY = -((viewportMouseY * 2.0f) - 1.0f);
        return pickFromNdc(frameInfo, ndcX, ndcY);
    }

    std::optional<entt::entity> PickingSystem::pick(FrameInfo& frameInfo, float mouseX, float mouseY, float aspectRatio) {
        (void) aspectRatio;
        float ndcX = (mouseX * 2.0f) - 1.0f;
        float ndcY = -((mouseY * 2.0f) - 1.0f);
        return pickFromNdc(frameInfo, ndcX, ndcY);
    }

    std::optional<entt::entity> PickingSystem::pickFromNdc(FrameInfo& frameInfo, float ndcX, float ndcY) {
        glm::vec3 rayDir    = unprojectToWorldRay(ndcX, ndcY,
            frameInfo.camera.getInverseView(),
            frameInfo.camera.getProjection());
        glm::vec3 rayOrigin = frameInfo.camera.getPosition();

        const auto& registry = frameInfo.scene->getRegistry();
        auto        view     = registry.view<ModelComponent, TransformComponent>();

        entt::entity bestEntity = entt::null;
        float        bestT      = FLT_MAX;

        for (auto entity : view) {
            const auto& transform = registry.get<TransformComponent>(entity);
            const auto& modelComp = registry.get<ModelComponent>(entity);
            const auto* model     = modelComp.model.get();

            if (!model || !model->getLocalBounds().isValid()) {
                continue;
            }

            const auto& localBounds = model->getLocalBounds();
            glm::vec3   worldMin    = localBounds.min;
            glm::vec3   worldMax    = localBounds.max;

            const glm::mat4& mat        = transform.modelTransform();
            glm::vec3        corners[8] = {
                glm::vec3(mat * glm::vec4(worldMin, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMax.x, worldMin.y, worldMin.z, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMin.x, worldMax.y, worldMin.z, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMax, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMin.x, worldMin.y, worldMax.z, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMax.x, worldMin.y, worldMax.z, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMin.x, worldMax.y, worldMax.z, 1.0f)),
                glm::vec3(mat * glm::vec4(worldMax.x, worldMax.y, worldMax.z, 1.0f)),
            };

            glm::vec3 worldAABBMin = glm::min(corners[0], glm::min(corners[1], glm::min(corners[2], corners[3])));
            worldAABBMin           = glm::min(worldAABBMin, glm::min(corners[4], glm::min(corners[5], glm::min(corners[6], corners[7]))));
            glm::vec3 worldAABBMax = glm::max(corners[0], glm::max(corners[1], glm::max(corners[2], corners[3])));
            worldAABBMax           = glm::max(worldAABBMax, glm::max(corners[4], glm::max(corners[5], glm::max(corners[6], corners[7]))));

            float tNear = 0.0f;
            if (intersectRayAABB(rayOrigin, rayDir, worldAABBMin, worldAABBMax, tNear)) {
                if (tNear < bestT) {
                    bestT      = tNear;
                    bestEntity = entity;
                }
            }
        }

        if (bestEntity == entt::null) {
            return std::nullopt;
        }

        return bestEntity;
    }

}  // namespace engine
