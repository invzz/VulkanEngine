#ifndef EDITOR_UI_SELECTION_RESOLVE_HPP
#define EDITOR_UI_SELECTION_RESOLVE_HPP
#include "Engine/Scene/components/SubMeshComponent.hpp"
#include "Engine/Scene/Scene.hpp"

#include <entt/entity/fwd.hpp>
namespace engine {
    /**
 * @brief Resolve the entity that transform-editing (gizmo, inspector panels)
 *        should operate on for the current selection.
 *
 * A sub-mesh selection (SubMeshComponent) has no own transform — it shares the
 * owning model's transform. Per the editor's selection model, selecting a
 * sub-mesh outlines just that sub-mesh (handled in SelectionMaskSystem) but
 * redirects transform editing to the parent model entity. Returns the input
 * entity unchanged when it is not a sub-mesh selection.
 */
    inline entt::entity resolveSelectionForTransform(const Scene& scene, entt::entity selected) {
        const auto& registry = scene.getRegistry();
        if (selected != entt::null && registry.valid(selected) &&
            registry.all_of<SubMeshComponent>(selected)) {
            return registry.get<SubMeshComponent>(selected).model;
        }
        return selected;
    }
}  // namespace engine
#endif  // EDITOR_UI_SELECTION_RESOLVE_HPP
