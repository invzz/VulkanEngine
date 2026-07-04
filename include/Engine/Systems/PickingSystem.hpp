#pragma once
#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <optional>
#include <unordered_map>
#include <vector>
namespace engine {
    class FrameInfo;
    class SpatialSystem;
    /**
     * @brief Viewport picking system with ray-triangle precision.
     *
     * Converts a viewport click to a world-space ray, then tests:
     *   1. Entities with ModelComponent — per-triangle intersection using
     *      the model's CPU-side collision geometry.
     *   2. Entities with CameraComponent (no ModelComponent) — projected
     *      screen-space point-radius test.
     *   3. Entities with PointLightComponent / SpotLightComponent — same
     *      screen-space radius test.
     *
     * All queries require a TransformComponent.  The closest hit wins.
     */
    class PickingSystem {
       public:
        PickingSystem()  = default;
        ~PickingSystem() = default;
        /**
         * @brief Set the spatial acceleration structure.
         *
         * If set, the BVH is rebuilt each frame and used as broadphase
         * culling before per-triangle intersection.
         */
        void setSpatialSystem(SpatialSystem* spatial) {
            spatial_ = spatial;
        }
        /**
         * @brief Pick the entity under the given normalized viewport coordinates.
         * @param frameInfo  Frame data (camera, scene, extent).
         * @param viewportX  Normalized X [0, 1] relative to viewport content.
         * @param viewportY  Normalized Y [0, 1] relative to viewport content.
         * @return The closest intersected entity, or std::nullopt.
         */
        std::optional<entt::entity> pickViewport(FrameInfo& frameInfo,
            float viewportX, float viewportY);

       private:
        struct Ray {
            glm::vec3 origin;
            glm::vec3 direction;
        };
        /** Build a world-space ray from normalized viewport coordinates. */
        Ray viewportToWorldRay(FrameInfo& frameInfo, float vpX, float vpY) const;
        /** Project a world-space point to viewport-normalized [0,1]². */
        glm::vec2 worldToViewport(FrameInfo& frameInfo, const glm::vec3& worldPos) const;
        bool      intersectRayAABB(const Ray& ray,
            const glm::vec3& aabbMin, const glm::vec3& aabbMax,
            float& tNear) const;
        bool      intersectRayTriangle(const Ray& ray,
            const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
            float& t, float& u, float& v) const;
        /** Screen-space pick radius in pixels (for cameras, lights, etc.). */
        static constexpr float kPickRadiusPx = 18.0f;
        SpatialSystem*         spatial_{nullptr};
    };
}  // namespace engine