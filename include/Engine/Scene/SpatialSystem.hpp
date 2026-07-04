#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SPATIALSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SPATIALSYSTEM_HPP
#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <optional>
#include <vector>

#include "ModelLib/Resources/Model.hpp"
namespace engine {
    /**
     * @brief Spatial acceleration structure for scene-level queries.
     *
     * Builds a BVH from entities with TransformComponent → world AABBs.
     * Leaf nodes store entt::entity handles. No Vulkan, no mesh data,
     * no rendering coupling.
     *
     * Currently rebuilds wholesale each call to rebuild().
     * Evolves to dynamic/static split when profiling demands it.
     */
    class SpatialSystem {
       public:
        /**
         * @brief Hit record returned by raycast queries.
         */
        struct RayHit {
            entt::entity entity;
            float        distance;
            glm::vec3    position;
        };
        /**
         * @brief World-space ray.
         */
        struct Ray {
            glm::vec3 origin;
            glm::vec3 direction;
        };
        /**
         * @brief Rebuild the BVH from the given ECS registry.
         *
         * Iterates all entities with TransformComponent, computes their
         * world-space AABB, and inserts them into the tree.
         *
         * @param registry EnTT registry to scan.
         * @param modelMap Map from entity → model for local bounds.
         */
        void rebuild(entt::registry&                             registry,
            const std::unordered_map<entt::entity, const AABB*>& modelBounds);
        /**
         * @brief Raycast against the BVH broadphase.
         *
         * Returns the closest entity whose world AABB is intersected.
         * Does NOT perform per-triangle or per-entity refinement — that
         * is the caller's responsibility (e.g. PickingSystem).
         *
         * @param ray World-space ray.
         * @return RayHit for the closest hit, or std::nullopt.
         */
        [[nodiscard]] std::optional<RayHit> raycast(const Ray& ray) const;
        /**
         * @brief AABB query against the BVH.
         *
         * Returns all entities whose world AABB overlaps the query AABB.
         *
         * @param bounds Query AABB.
         * @return List of overlapping entities.
         */
        [[nodiscard]] std::vector<entt::entity> queryAABB(const AABB& bounds) const;

       private:
        /**
         * @brief BVH node.
         *
         * Internal nodes: left != UINT32_MAX, right != UINT32_MAX.
         * Leaf nodes: entity != entt::null, left == UINT32_MAX.
         */
        struct Node {
            AABB         bounds;
            uint32_t     left   = UINT32_MAX;
            uint32_t     right  = UINT32_MAX;
            entt::entity entity = entt::null;
        };
        /**
         * @brief Insert a leaf into the tree at the given node index.
         */
        uint32_t insertLeaf(uint32_t nodeIdx, AABB bounds, entt::entity entity);
        /**
         * @brief Build the tree from leaf data via SAH-like split.
         */
        void buildTree();
        /**
         * @brief Recursive tree builder.
         */
        void buildRecursive(uint32_t nodeIdx, uint32_t leafStart, uint32_t leafCount);
        /**
         * @brief Ray-AABB test returning t-entry distance.
         */
        bool intersectRayAABB(const Ray& ray, const AABB& aabb, float& tNear) const;
        /**
         * @brief Raycast traversal against the BVH.
         */
        std::optional<RayHit> raycastTraversal(const Ray& ray) const;
        /**
         * @brief AABB query traversal.
         */
        void                                       queryAABBT(const AABB& bounds, std::vector<entt::entity>& out) const;
        std::vector<Node>                          nodes_;
        std::vector<std::pair<entt::entity, AABB>> leaves_;
        uint32_t                                   root_ = UINT32_MAX;
    };
}  // namespace engine
#endif
