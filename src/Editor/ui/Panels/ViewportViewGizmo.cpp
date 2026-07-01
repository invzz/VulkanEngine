#include "Editor/ui/Panels/ViewportViewGizmo.hpp"

#include <algorithm>
#include <entt/entt.hpp>

#define IMVIEWGUIZMO_IMPLEMENTATION
#include "ImViewGuizmo.h"
#undef IMVIEWGUIZMO_IMPLEMENTATION

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/ViewportGizmoCoordinates.hpp"

namespace {

    struct ViewOrbitContext {
        bool      orbitAroundSelection{false};
        glm::vec3 pivot{0.0f};
        float     minOrbitDistance{0.5f};
    };

    void clampToOrbitShell(glm::vec3& cameraPos, const glm::quat& cameraRot, const glm::vec3& pivot, float minOrbitDistance) {
        glm::vec3 orbitOffset   = cameraPos - pivot;
        float     orbitDistance = glm::length(orbitOffset);
        if (orbitDistance < minOrbitDistance) {
            const glm::vec3 orbitDir = (orbitDistance > 1e-4f)
                                           ? (orbitOffset / orbitDistance)
                                           : (cameraRot * glm::vec3(0.0f, 0.0f, 1.0f));
            cameraPos                = pivot + orbitDir * minOrbitDistance;
        }
    }

    ViewOrbitContext resolveViewOrbitContext(
        entt::registry&          registry,
        const engine::FrameInfo& frameInfo,
        const glm::vec3&         cameraPos) {
        ViewOrbitContext context{};
        context.pivot = cameraPos;

        const bool hasValidSelectedPivot =
            registry.valid(frameInfo.selectedEntity) &&
            frameInfo.selectedEntity != frameInfo.cameraEntity &&
            registry.all_of<engine::TransformComponent, engine::ModelComponent>(frameInfo.selectedEntity);
        context.orbitAroundSelection = frameInfo.viewGizmoOrbitSelected && hasValidSelectedPivot;

        if (!context.orbitAroundSelection) {
            return context;
        }

        const auto& selectedTransform = registry.get<engine::TransformComponent>(frameInfo.selectedEntity);
        context.pivot                 = selectedTransform.translation;

        const auto& modelComp = registry.get<engine::ModelComponent>(frameInfo.selectedEntity);
        if (!modelComp.model) {
            context.orbitAroundSelection = false;
            return context;
        }

        const engine::AABB& localBounds = modelComp.model->getLocalBounds();
        if (!localBounds.isValid()) {
            context.orbitAroundSelection = false;
            return context;
        }

        const engine::AABB worldBounds = engine::transformAABB(localBounds, selectedTransform.modelTransform());
        context.pivot                  = worldBounds.center();
        const float boundRadius        = glm::length(worldBounds.extents());
        context.minOrbitDistance       = std::max(0.5f, boundRadius * 2.0f);

        return context;
    }

}  // namespace

namespace engine {

    void ViewportViewGizmo::render(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size) {
        if (frameInfo.viewportMode == ViewportMode::Navigation) {
            return;
        }

        auto& registry = frameInfo.scene->getRegistry();

        if (!registry.valid(frameInfo.cameraEntity) || !registry.all_of<TransformComponent>(frameInfo.cameraEntity)) {
            return;
        }

        constexpr ImVec2 kGizmoOffset = ImVec2(80.0f, 18.0f);
        ImVec2           gizmoPos     = ImVec2(topLeft.x + kGizmoOffset.x, topLeft.y + size.y - kGizmoOffset.y - 120.0f);

        auto&                  cameraTransform   = registry.get<TransformComponent>(frameInfo.cameraEntity);
        glm::vec3              cameraPos         = cameraTransform.translation;
        glm::vec3              originalCameraPos = cameraPos;
        glm::quat              cameraRot         = Camera::rotationQuatFromYXZ(cameraTransform.rotation);
        const ViewOrbitContext orbitContext      = resolveViewOrbitContext(registry, frameInfo, cameraPos);

        if (orbitContext.orbitAroundSelection) {
            clampToOrbitShell(cameraPos, cameraRot, orbitContext.pivot, orbitContext.minOrbitDistance);
        }

        glm::vec3 cameraPosG = engine::editor::viewport_gizmo::toGizmoSpace(cameraPos);
        glm::quat cameraRotG = engine::editor::viewport_gizmo::toGizmoSpace(cameraRot);
        glm::vec3 pivotG     = engine::editor::viewport_gizmo::toGizmoSpace(orbitContext.pivot);

        if (ImViewGuizmo::Rotate(cameraPosG, cameraRotG, pivotG, gizmoPos)) {
            cameraPos = engine::editor::viewport_gizmo::fromGizmoSpace(cameraPosG);
            cameraRot = engine::editor::viewport_gizmo::fromGizmoSpace(cameraRotG);

            if (!orbitContext.orbitAroundSelection) {
                cameraPos = originalCameraPos;
            } else {
                clampToOrbitShell(cameraPos, cameraRot, orbitContext.pivot, orbitContext.minOrbitDistance);
            }
            const glm::vec3 yxz         = Camera::rotationYXZFromQuat(cameraRot);
            cameraTransform.translation = cameraPos;
            cameraTransform.rotation    = yxz;
        }
    }

}  // namespace engine
