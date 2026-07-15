#include "Editor/ui/Panels/ViewportObjectGizmo.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <ImGuizmo.h>

#include <cmath>

#include "Editor/ui/SelectionResolve.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
namespace engine {
    void ViewportObjectGizmo::render(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size) {
        if (frameInfo.selectedEntity == entt::null || !frameInfo.gizmoEnabled) {
            return;
        }
        auto& registry = frameInfo.scene->getRegistry();
        // A sub-mesh selection shares the parent model's transform; edit that.
        const entt::entity target = resolveSelectionForTransform(*frameInfo.scene, frameInfo.selectedEntity);
        if (!registry.valid(target)) {
            return;
        }
        if (!registry.all_of<TransformComponent>(target)) {
            return;
        }
        auto& transform = registry.get<TransformComponent>(target);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(topLeft.x, topLeft.y, size.x, size.y);
        const glm::mat4& view = frameInfo.camera.getView();
        const glm::mat4& proj = frameInfo.camera.getProjection();
        glm::mat4        projGL(1.0f);
        projGL[2][2]           = 2.0f;
        projGL[3][2]           = -1.0f;
        glm::mat4 projForGizmo = projGL * proj;
        projForGizmo[1][1] *= -1.0f;
        glm::mat4 objectMatrix = transform.modelTransform();
        float     matrix[16];
        memcpy(matrix, glm::value_ptr(objectMatrix), sizeof(matrix));
        auto operation = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        auto mode      = static_cast<ImGuizmo::MODE>(frameInfo.gizmoMode);
        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projForGizmo),
            operation,
            mode,
            matrix);
        if (ImGuizmo::IsUsing()) {
            glm::mat4 newMatrix;
            memcpy(&newMatrix, matrix, sizeof(matrix));
            transform.translation = glm::vec3(newMatrix[3]);
            glm::vec3 newScale(
                glm::length(glm::vec3(newMatrix[0])),
                glm::length(glm::vec3(newMatrix[1])),
                glm::length(glm::vec3(newMatrix[2])));
            glm::mat4 R = newMatrix;
            if (newScale.x > 1e-6f) {
                R[0] /= newScale.x;
            }
            if (newScale.y > 1e-6f) {
                R[1] /= newScale.y;
            }
            if (newScale.z > 1e-6f) {
                R[2] /= newScale.z;
            }
            const float s2 = -R[2][1];
            const float c2 = std::sqrt(1.0f - (s2 * s2));
            if (std::abs(c2) > 1e-6f) {
                transform.rotation.x = std::asin(s2);
                transform.rotation.y = std::atan2(R[2][0], R[2][2]);
                transform.rotation.z = std::atan2(R[0][1], R[1][1]);
            } else {
                transform.rotation.x = (s2 > 0.0f) ? glm::half_pi<float>() : -glm::half_pi<float>();
                transform.rotation.y = std::atan2(-R[0][2], R[0][0]);
                transform.rotation.z = 0.0f;
            }
            transform.scale = newScale;
        }
    }
}  // namespace engine
