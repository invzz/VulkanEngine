#include "Engine/Scene/SpatialSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    // ==========================================================================
    // Public API
    // ==========================================================================

    void SpatialSystem::rebuild(entt::registry& registry,
                                const std::unordered_map<entt::entity, const AABB*>& modelBounds) {
        // Collect leaves: all entities with TransformComponent.
        leaves_.clear();
        nodes_.clear();

        // Iterate all entities with TransformComponent.
        // We also check for ModelComponent to get local bounds.
        auto view = registry.view<TransformComponent>();
        for (auto entity : view) {
            const auto& transform = registry.get<TransformComponent>(entity);

            AABB bounds;
            bool hasBounds = false;

            // Try to get model local bounds first.
            if (const auto* localBoundsPtr = modelBounds.count(entity) ? modelBounds.at(entity) : nullptr;
                localBoundsPtr && localBoundsPtr->isValid()) {
                // Transform local AABB to world space.
                const glm::mat4& worldMat = transform.modelTransform();
                bounds = transformAABB(*localBoundsPtr, worldMat);
                hasBounds = true;
            }

            if (!hasBounds) {
                // No model bounds — use the transform position as a point
                // and expand to a default size (1 unit).
                const glm::vec3& pos = transform.translation;
                bounds.min = pos - glm::vec3(0.5f);
                bounds.max = pos + glm::vec3(0.5f);
            }

            leaves_.emplace_back(entity, bounds);
        }

        if (leaves_.empty()) {
            root_ = UINT32_MAX;
            return;
        }

        // Initialize root node containing all leaves.
        AABB rootBounds;
        rootBounds.min = glm::vec3(std::numeric_limits<float>::max());
        rootBounds.max = glm::vec3(-std::numeric_limits<float>::max());
        for (const auto& [ent, bnd] : leaves_) {
            rootBounds.min = glm::min(rootBounds.min, bnd.min);
            rootBounds.max = glm::max(rootBounds.max, bnd.max);
        }

        nodes_.clear();
        nodes_.emplace_back();
        nodes_.back().bounds = rootBounds;
        root_ = 0;

        buildTree();
    }

    std::optional<SpatialSystem::RayHit> SpatialSystem::raycast(const Ray& ray) const {
        return raycastTraversal(ray);
    }

    std::vector<entt::entity> SpatialSystem::queryAABB(const AABB& bounds) const {
        std::vector<entt::entity> out;
        queryAABBT(bounds, out);
        return out;
    }

    // ==========================================================================
    // BVH construction
    // ==========================================================================

    uint32_t SpatialSystem::insertLeaf(uint32_t nodeIdx, AABB bounds, entt::entity entity) {
        // Expand node storage if needed.
        if (nodeIdx >= nodes_.size()) {
            nodes_.resize(nodeIdx + 1);
        }

        // This node becomes a leaf.
        nodes_[nodeIdx].bounds = bounds;
        nodes_[nodeIdx].entity = entity;
        nodes_[nodeIdx].left   = UINT32_MAX;
        nodes_[nodeIdx].right  = UINT32_MAX;

        return nodeIdx;
    }

    void SpatialSystem::buildTree() {
        if (leaves_.empty()) return;

        buildRecursive(root_, 0, static_cast<uint32_t>(leaves_.size()));
    }

    void SpatialSystem::buildRecursive(uint32_t nodeIdx, uint32_t leafStart, uint32_t leafCount) {
        if (leafCount <= 1) {
            // Single leaf — make this node a leaf.
            if (leafCount == 1) {
                const auto& [ent, bnd] = leaves_[leafStart];
                nodes_[nodeIdx].bounds = bnd;
                nodes_[nodeIdx].entity = ent;
                nodes_[nodeIdx].left   = UINT32_MAX;
                nodes_[nodeIdx].right  = UINT32_MAX;
            }
            return;
        }

        // Compute bounds of all leaves in this range.
        AABB bounds;
        bounds.min = glm::vec3(std::numeric_limits<float>::max());
        bounds.max = glm::vec3(-std::numeric_limits<float>::max());
        for (uint32_t i = 0; i < leafCount; ++i) {
            bounds.min = glm::min(bounds.min, leaves_[leafStart + i].second.min);
            bounds.max = glm::max(bounds.max, leaves_[leafStart + i].second.max);
        }
        nodes_[nodeIdx].bounds = bounds;

        // Find the axis with the largest extent.
        const glm::vec3 extents = bounds.max - bounds.min;
        int axis = 0;
        if (extents.y > extents.x && extents.y > extents.z) axis = 1;
        else if (extents.z > extents.x) axis = 2;

        // Sort leaves by the midpoint on that axis.
        const float mid = (bounds.min[axis] + bounds.max[axis]) * 0.5f;
        std::sort(leaves_.begin() + leafStart,
                  leaves_.begin() + leafStart + leafCount,
                  [axis, mid](const auto& a, const auto& b) {
                      const float aMid = (a.second.min[axis] + a.second.max[axis]) * 0.5f;
                      const float bMid = (b.second.min[axis] + b.second.max[axis]) * 0.5f;
                      return aMid < bMid;
                  });

        // Split at the midpoint.
        const uint32_t leftCount = leafCount / 2;
        const uint32_t rightCount = leafCount - leftCount;

        // Create left child.
        const uint32_t leftChild = static_cast<uint32_t>(nodes_.size());
        nodes_.emplace_back();
        nodes_[nodeIdx].left = leftChild;

        // Create right child.
        const uint32_t rightChild = static_cast<uint32_t>(nodes_.size());
        nodes_.emplace_back();
        nodes_[nodeIdx].right = rightChild;

        // Recurse.
        buildRecursive(leftChild, leafStart, leftCount);
        buildRecursive(rightChild, leafStart + leftCount, rightCount);
    }

    // ==========================================================================
    // Ray-AABB intersection
    // ==========================================================================

    bool SpatialSystem::intersectRayAABB(const Ray& ray, const AABB& aabb, float& tNear) const {
        const glm::vec3 invDir = glm::vec3(1.0f) / ray.direction;
        const glm::vec3 tMin   = (aabb.min - ray.origin) * invDir;
        const glm::vec3 tMax   = (aabb.max - ray.origin) * invDir;

        const glm::vec3 t1 = glm::min(tMin, tMax);
        const glm::vec3 t2 = glm::max(tMin, tMax);

        const float tEntry = glm::max(glm::max(t1.x, t1.y), t1.z);
        const float tExit  = glm::min(glm::min(t2.x, t2.y), t2.z);

        if (tEntry > tExit || tExit < 0.0f) return false;

        tNear = (tEntry > 0.0f) ? tEntry : 0.0f;
        return true;
    }

    // ==========================================================================
    // Raycast traversal
    // ==========================================================================

    std::optional<SpatialSystem::RayHit> SpatialSystem::raycastTraversal(const Ray& ray) const {
        if (root_ == UINT32_MAX) return std::nullopt;

        std::optional<RayHit> bestHit;
        float bestT = std::numeric_limits<float>::max();

        // Iterative traversal using a stack.
        struct StackEntry {
            uint32_t nodeIdx;
            float    tMin;  // t-entry for this node
        };
        std::vector<StackEntry> stack;
        stack.reserve(64);

        float rootT = 0.0f;
        if (!intersectRayAABB(ray, nodes_[root_].bounds, rootT)) return std::nullopt;

        stack.emplace_back(root_, rootT);

        while (!stack.empty()) {
            const StackEntry entry = stack.back();
            stack.pop_back();

            const uint32_t nodeIdx = entry.nodeIdx;
            const float    tMin    = entry.tMin;

            // If this node is a leaf.
            if (nodes_[nodeIdx].left == UINT32_MAX) {
                // Leaf node — check if we already found a closer hit.
                if (tMin >= bestT) continue;

                const float tEntry = tMin;
                if (bestHit.has_value() && tEntry >= bestHit->distance) continue;

                // Compute hit position.
                const glm::vec3 hitPos = ray.origin + ray.direction * tEntry;

                bestHit = RayHit{nodes_[nodeIdx].entity, tEntry, hitPos};
                bestT = tEntry;
                continue;
            }

            // Internal node — traverse children.
            // Push the farther child first so the closer child is processed first.
            const uint32_t left  = nodes_[nodeIdx].left;
            const uint32_t right = nodes_[nodeIdx].right;

            float tLeft  = std::numeric_limits<float>::max();
            float tRight = std::numeric_limits<float>::max();

            const bool leftHit  = intersectRayAABB(ray, nodes_[left].bounds, tLeft);
            const bool rightHit = intersectRayAABB(ray, nodes_[right].bounds, tRight);

            if (leftHit && rightHit) {
                // Both hit — push farther first.
                if (tLeft < tRight) {
                    stack.emplace_back(left, tLeft);
                    stack.emplace_back(right, tRight);
                } else {
                    stack.emplace_back(right, tRight);
                    stack.emplace_back(left, tLeft);
                }
            } else if (leftHit) {
                stack.emplace_back(left, tLeft);
            } else if (rightHit) {
                stack.emplace_back(right, tRight);
            }
        }

        return bestHit;
    }

    // ==========================================================================
    // AABB query traversal
    // ==========================================================================

    void SpatialSystem::queryAABBT(const AABB& bounds, std::vector<entt::entity>& out) const {
        if (root_ == UINT32_MAX) return;

        // Use a simple iterative traversal.
        struct StackEntry {
            uint32_t nodeIdx;
        };
        std::vector<StackEntry> stack;
        stack.reserve(64);
        stack.emplace_back(root_);

        while (!stack.empty()) {
            const uint32_t nodeIdx = stack.back().nodeIdx;
            stack.pop_back();

            // Check AABB overlap.
            const AABB& nodeBounds = nodes_[nodeIdx].bounds;
            const bool overlaps =
                nodeBounds.min.x <= bounds.max.x && nodeBounds.max.x >= bounds.min.x &&
                nodeBounds.min.y <= bounds.max.y && nodeBounds.max.y >= bounds.min.y &&
                nodeBounds.min.z <= bounds.max.z && nodeBounds.max.z >= bounds.min.z;

            if (!overlaps) continue;

            // Leaf node.
            if (nodes_[nodeIdx].left == UINT32_MAX) {
                out.push_back(nodes_[nodeIdx].entity);
                continue;
            }

            // Internal node — push children.
            stack.emplace_back(nodes_[nodeIdx].left);
            stack.emplace_back(nodes_[nodeIdx].right);
        }
    }

}  // namespace engine
