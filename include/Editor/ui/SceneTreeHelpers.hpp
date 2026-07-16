#pragma once

#include <imgui.h>

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {
    class Scene;
}

namespace engine::ui {

    /// Parent -> children index built once per section draw, turning what was an
    /// O(n^2) hierarchy walk (re-scanning the whole ChildComponent view for every
    /// node) into O(n).
    class ChildrenIndex {
       public:
        explicit ChildrenIndex(const entt::registry& registry);

        const std::vector<entt::entity>& childrenOf(entt::entity parent) const;
        bool                             hasChildren(entt::entity parent) const;

       private:
        std::unordered_map<entt::entity, std::vector<entt::entity>> byParent_;
    };

    /// Display name for an entity: its NameComponent, or "Object <id>".
    std::string entityDisplayName(const entt::registry& registry, entt::entity entity);

    /// Draw a single entity row: selectable label + per-type inline actions
    /// (set-active-camera / delete).
    void drawEntityRow(entt::entity entity,
        const char*                 icon,
        ImVec4                      color,
        FrameInfo&                  frameInfo,
        const entt::registry&       registry,
        std::vector<entt::entity>&  toDelete);

    /// Draw the node entity that instantiates a sub-mesh as an "instance"
    /// under it (model -> submeshes -> instances).
    void drawInstancesFor(entt::entity modelEntity,
        uint32_t                       subMeshIndex,
        const ChildrenIndex&           childrenIndex,
        const entt::registry&          registry,
        FrameInfo&                     frameInfo,
        std::vector<entt::entity>&     toDelete);

    /// Draw sub-mesh (glTF primitive) children of a parent model entity. Each
    /// sub-mesh is a tree node; under it, the node entities that instantiate it
    /// are shown as "instances".
    void drawSubMeshChildren(entt::entity parent,
        const ChildrenIndex&              childrenIndex,
        const entt::registry&             registry,
        FrameInfo&                        frameInfo,
        std::vector<entt::entity>&        toDelete);

    /// Draw light children of a parent entity as flat selectable rows.
    void drawLightChildren(entt::entity parent,
        const ChildrenIndex&            childrenIndex,
        const entt::registry&           registry,
        FrameInfo&                      frameInfo,
        std::vector<entt::entity>&      toDelete);

}  // namespace engine::ui
