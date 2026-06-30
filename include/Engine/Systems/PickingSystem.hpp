#pragma once

#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <optional>

namespace engine {

    class Scene;
    class FrameInfo;

    /**
     * @brief CPU-based ray-AABB picking system for viewport selection.
     * Converts mouse screen coordinates to a world-space ray and tests
     * intersection against all entities with a ModelComponent + TransformComponent.
     */
    class PickingSystem {
       public:
        PickingSystem() = default;

        /**
         * @brief Perform picking for the given normalized viewport coordinates.
         * @param frameInfo Contains the view/projection matrices and scene.
         * @param viewportMouseX X coordinate in [0, 1] relative to the viewport content area.
         * @param viewportMouseY Y coordinate in [0, 1] relative to the viewport content area.
         * @return The closest selected entity, or std::nullopt if nothing picked.
         */
        std::optional<entt::entity> pickViewport(FrameInfo& frameInfo, float viewportMouseX, float viewportMouseY);

        /**
         * @brief Legacy overload using raw NDC parameters.
         */
        std::optional<entt::entity> pick(FrameInfo& frameInfo, float mouseX, float mouseY, float aspectRatio);

       private:
        /**
         * @brief Test ray-AABB intersection.
         */
        bool intersectRayAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& tNear) const;

        /**
         * @brief Generate a world-space ray from normalized screen coordinates.
         */
        glm::vec3 unprojectToWorldRay(float ndcX, float ndcY, glm::mat4 invProj, glm::mat4 invView) const;

        std::optional<entt::entity> pickFromNdc(FrameInfo& frameInfo, float ndcX, float ndcY);
    };

}  // namespace engine
