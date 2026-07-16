#include "Editor/ui/SceneTree.hpp"

#include <imgui.h>

#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/SubMeshComponent.hpp"

#include "Editor/ui/Theme.hpp"
#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"

namespace engine::ui {

    SceneTree::SceneTree(const entt::registry& registry)
        : registry_(registry), children_(registry) {}

    void SceneTree::DrawEntity(entt::entity entity,
        const char*                         icon,
        ImVec4                              color,
        FrameInfo&                          frameInfo,
        std::vector<entt::entity>&          toDelete) {
        drawEntityRow(entity, icon, color, frameInfo, registry_, toDelete);

        bool hasLights = false;
        for (auto child : children_.childrenOf(entity)) {
            if (registry_.all_of<PointLightComponent>(child) ||
                registry_.all_of<DirectionalLightComponent>(child) ||
                registry_.all_of<SpotLightComponent>(child)) {
                hasLights = true;
            }
        }

        if (hasLights) {
            if (UI::TreeNode(ICON_FA_LIGHTBULB, "Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawLightChildren(entity, children_, registry_, frameInfo, toDelete);
                ImGui::TreePop();
            }
        }
        bool hasSubMeshes = false;
        for (auto child : children_.childrenOf(entity)) {
            if (registry_.all_of<SubMeshComponent>(child)) {
                hasSubMeshes = true;
                break;
            }
        }
        if (hasSubMeshes) {
            if (UI::TreeNode(ICON_FA_SHAPES, "Submeshes", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawSubMeshChildren(entity, children_, registry_, frameInfo, toDelete);
                ImGui::TreePop();
            }
        }
    }

}  // namespace engine::ui
