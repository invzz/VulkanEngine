#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <memory>
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
         * @brief Perform picking for the given mouse coordinates.
         * @param frameInfo Contains the view/projection matrices and scene.
         * @param mouseX X coordinate in [0, 1] (viewport space).
         * @param mouseY Y coordinate in [0, 1] (viewport space).
         * @param aspectRatio Viewport aspect ratio (width / height).
         * @return The closest selected entity, or std::nullopt if nothing picked.
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
    };

}  // namespace engine
