#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_SUBMESHCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_SUBMESHCOMPONENT_HPP
#include <cstdint>

#include <entt/entt.hpp>
namespace engine {
    /**
 * @brief Marks a regenerated child entity that represents one sub-mesh
 *        (glTF primitive) of a model for sub-mesh-level selection.
 *
 * Sub-mesh entities are NOT authored or serialized: they are recreated by
 * ModelLoadProcessor whenever a model is loaded, one per Model::SubMesh.
 * They carry no own transform — the owning model's transform applies.
 * Selection of a sub-mesh entity outlines that single sub-mesh and, per the
 * editor's selection model, redirects gizmo/inspector editing to the parent
 * model entity (see resolveSelectionForTransform).
 */
    struct SubMeshComponent {
        entt::entity model{entt::null};
        uint32_t     subMeshIndex{0};
    };
}  // namespace engine
#endif
