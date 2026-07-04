#include "Engine/Systems/PickingSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <limits>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SpatialSystem.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
namespace engine {
    std::optional<entt::entity> PickingSystem::pickViewport(FrameInfo& frameInfo,
        float viewportX, float viewportY) {
        Ray                       ray      = viewportToWorldRay(frameInfo, viewportX, viewportY);
        auto&                     registry = frameInfo.scene->getRegistry();
        const float               vpW      = static_cast<float>(frameInfo.extent.width);
        const float               vpH      = static_cast<float>(frameInfo.extent.height);
        std::vector<entt::entity> candidates;
        bool                      useBvh = false;
        if (spatial_) {
            std::unordered_map<entt::entity, const AABB*> modelBounds;
            auto                                          modelView = registry.view<ModelComponent>();
            for (auto e : modelView) {
                const auto& mc = registry.get<ModelComponent>(e);
                if (mc.model) {
                    const auto* lb = &mc.model->getLocalBounds();
                    if (lb->isValid()) {
                        modelBounds[e] = lb;
                    }
                }
            }
            spatial_->rebuild(registry, std::move(modelBounds));
            if (auto hit = spatial_->raycast({ray.origin, ray.direction})) {
                candidates.push_back(hit->entity);
                useBvh = true;
            }
        }
        entt::entity bestEntity = entt::null;
        float        bestT      = std::numeric_limits<float>::max();
        {
            auto modelView = registry.view<ModelComponent, TransformComponent>();
            for (auto entity : modelView) {
                if (useBvh && candidates[0] != entity)
                    continue;
                const auto& transform = registry.get<TransformComponent>(entity);
                const auto& modelComp = registry.get<ModelComponent>(entity);
                const auto* model     = modelComp.model.get();
                if (!model)
                    continue;
                const auto& localBounds = model->getLocalBounds();
                if (!localBounds.isValid())
                    continue;
                const glm::mat4& worldMat   = transform.modelTransform();
                glm::vec3        corners[8] = {
                    glm::vec3(worldMat * glm::vec4(localBounds.min.x, localBounds.min.y, localBounds.min.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.max.x, localBounds.min.y, localBounds.min.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.min.x, localBounds.max.y, localBounds.min.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.max.x, localBounds.max.y, localBounds.min.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.min.x, localBounds.min.y, localBounds.max.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.max.x, localBounds.min.y, localBounds.max.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.min.x, localBounds.max.y, localBounds.max.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(localBounds.max.x, localBounds.max.y, localBounds.max.z, 1.0f)),
                };
                glm::vec3 worldAABBMin = glm::min(corners[0],
                    glm::min(corners[1], glm::min(corners[2], corners[3])));
                worldAABBMin           = glm::min(worldAABBMin,
                    glm::min(corners[4], glm::min(corners[5], glm::min(corners[6], corners[7]))));
                glm::vec3 worldAABBMax = glm::max(corners[0],
                    glm::max(corners[1], glm::max(corners[2], corners[3])));
                worldAABBMax           = glm::max(worldAABBMax,
                    glm::max(corners[4], glm::max(corners[5], glm::max(corners[6], corners[7]))));
                float aabbT            = 0.0f;
                if (!intersectRayAABB(ray, worldAABBMin, worldAABBMax, aabbT))
                    continue;
                if (aabbT >= bestT)
                    continue;
                glm::mat4 invWorld = glm::inverse(worldMat);
                Ray       localRay;
                localRay.origin    = glm::vec3(invWorld * glm::vec4(ray.origin, 1.0f));
                localRay.direction = glm::normalize(
                    glm::vec3(invWorld * glm::vec4(ray.origin + ray.direction, 1.0f)) - localRay.origin);
                const auto& vertices = model->getCollisionVertices();
                const auto& indices  = model->getCollisionIndices();
                if (vertices.empty() || indices.empty())
                    continue;
                float bestLocalT = std::numeric_limits<float>::max();
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    const glm::vec3& v0   = vertices[indices[i]];
                    const glm::vec3& v1   = vertices[indices[i + 1]];
                    const glm::vec3& v2   = vertices[indices[i + 2]];
                    float            tTri = 0.0f, u = 0.0f, v = 0.0f;
                    if (intersectRayTriangle(localRay, v0, v1, v2, tTri, u, v)) {
                        if (tTri < bestLocalT) {
                            bestLocalT = tTri;
                        }
                    }
                }
                if (bestLocalT < std::numeric_limits<float>::max()) {
                    float worldHitT = aabbT;
                    if (bestLocalT < bestT) {
                        bestT      = worldHitT;
                        bestEntity = entity;
                    }
                }
            }
        }
        {
            auto view = registry.view<CameraComponent, TransformComponent>();
            for (auto entity : view) {
                if (registry.all_of<ModelComponent>(entity))
                    continue;
                const auto& transform = registry.get<TransformComponent>(entity);
                glm::vec2   sp        = worldToViewport(frameInfo, transform.translation);
                glm::vec2   click(viewportX, viewportY);
                float       dist = glm::length(sp - click) * std::max(vpW, vpH);
                if (dist < kPickRadiusPx && dist < bestT) {
                    bestT      = dist;
                    bestEntity = entity;
                }
            }
        }
        {
            auto pointView = registry.view<PointLightComponent, TransformComponent>();
            for (auto entity : pointView) {
                if (registry.all_of<ModelComponent>(entity))
                    continue;
                const auto& transform = registry.get<TransformComponent>(entity);
                glm::vec2   sp        = worldToViewport(frameInfo, transform.translation);
                glm::vec2   click(viewportX, viewportY);
                float       dist = glm::length(sp - click) * std::max(vpW, vpH);
                if (dist < kPickRadiusPx && dist < bestT) {
                    bestT      = dist;
                    bestEntity = entity;
                }
            }
            auto spotView = registry.view<SpotLightComponent, TransformComponent>();
            for (auto entity : spotView) {
                if (registry.all_of<ModelComponent>(entity))
                    continue;
                const auto& transform = registry.get<TransformComponent>(entity);
                glm::vec2   sp        = worldToViewport(frameInfo, transform.translation);
                glm::vec2   click(viewportX, viewportY);
                float       dist = glm::length(sp - click) * std::max(vpW, vpH);
                if (dist < kPickRadiusPx && dist < bestT) {
                    bestT      = dist;
                    bestEntity = entity;
                }
            }
            auto dirView = registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : dirView) {
                if (registry.all_of<ModelComponent>(entity))
                    continue;
                const auto& transform = registry.get<TransformComponent>(entity);
                glm::vec2   sp        = worldToViewport(frameInfo, transform.translation);
                glm::vec2   click(viewportX, viewportY);
                float       dist = glm::length(sp - click) * std::max(vpW, vpH);
                if (dist < kPickRadiusPx && dist < bestT) {
                    bestT      = dist;
                    bestEntity = entity;
                }
            }
        }
        if (bestEntity == entt::null)
            return std::nullopt;
        return bestEntity;
    }
    PickingSystem::Ray PickingSystem::viewportToWorldRay(FrameInfo& frameInfo,
        float vpX, float vpY) const {
        float     ndcX    = vpX * 2.0f - 1.0f;
        float     ndcY    = -(vpY * 2.0f - 1.0f);
        glm::mat4 invProj = glm::inverse(frameInfo.camera.getProjection());
        glm::mat4 invView = frameInfo.camera.getInverseView();
        glm::vec4 clipNear(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 clipFar(ndcX, ndcY, 1.0f, 1.0f);
        glm::vec4 eyeNear = invProj * clipNear;
        glm::vec4 eyeFar  = invProj * clipFar;
        eyeNear /= eyeNear.w;
        eyeFar /= eyeFar.w;
        glm::vec3 worldNear = glm::vec3(invView * eyeNear);
        glm::vec3 worldFar  = glm::vec3(invView * eyeFar);
        Ray       ray;
        ray.origin    = frameInfo.camera.getPosition();
        ray.direction = glm::normalize(worldFar - worldNear);
        return ray;
    }
    glm::vec2 PickingSystem::worldToViewport(FrameInfo& frameInfo,
        const glm::vec3&                                worldPos) const {
        glm::vec4 clipPos = frameInfo.camera.getProjection() * frameInfo.camera.getView() * glm::vec4(worldPos, 1.0f);
        if (std::abs(clipPos.w) < 1e-8f)
            return glm::vec2(-1.0f, -1.0f);
        float ndcX = clipPos.x / clipPos.w;
        float ndcY = clipPos.y / clipPos.w;
        float vpX  = (ndcX + 1.0f) * 0.5f;
        float vpY  = 1.0f - (ndcY + 1.0f) * 0.5f;
        return glm::vec2(vpX, vpY);
    }
    bool PickingSystem::intersectRayAABB(const Ray& ray,
        const glm::vec3& aabbMin, const glm::vec3& aabbMax,
        float& tNear) const {
        glm::vec3 invDir = glm::vec3(1.0f) / ray.direction;
        glm::vec3 tMin   = (aabbMin - ray.origin) * invDir;
        glm::vec3 tMax   = (aabbMax - ray.origin) * invDir;
        glm::vec3 t1     = glm::min(tMin, tMax);
        glm::vec3 t2     = glm::max(tMin, tMax);
        float     tEntry = glm::max(glm::max(t1.x, t1.y), t1.z);
        float     tExit  = glm::min(glm::min(t2.x, t2.y), t2.z);
        if (tEntry > tExit || tExit < 0.0f)
            return false;
        tNear = (tEntry > 0.0f) ? tEntry : 0.0f;
        return true;
    }
    bool PickingSystem::intersectRayTriangle(const Ray& ray,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
        float& t, float& u, float& v) const {
        const glm::vec3 edge1 = v1 - v0;
        const glm::vec3 edge2 = v2 - v0;
        const glm::vec3 h     = glm::cross(ray.direction, edge2);
        const float     a     = glm::dot(edge1, h);
        if (std::abs(a) < 1e-8f)
            return false;
        const float     f  = 1.0f / a;
        const glm::vec3 s  = ray.origin - v0;
        const float     uu = f * glm::dot(s, h);
        if (uu < 0.0f || uu > 1.0f)
            return false;
        const glm::vec3 q  = glm::cross(s, edge1);
        const float     vv = f * glm::dot(ray.direction, q);
        if (vv < 0.0f || uu + vv > 1.0f)
            return false;
        const float tt = f * glm::dot(edge2, q);
        if (tt < 0.0f)
            return false;
        t = tt;
        u = uu;
        v = vv;
        return true;
    }
}  // namespace engine