#include "Editor/ui/SceneTreeHelpers.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/ChildComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/NodeIndexComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/SubMeshComponent.hpp"

#include "Editor/ui/Theme.hpp"
#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
#include "ModelLib/Resources/Model.hpp"

namespace engine::ui {

    ChildrenIndex::ChildrenIndex(const entt::registry& registry) {
        auto view = registry.view<ChildComponent>();
        for (auto child : view) {
            const auto parent = registry.get<ChildComponent>(child).parent;
            byParent_[parent].push_back(child);
        }
    }

    const std::vector<entt::entity>& ChildrenIndex::childrenOf(entt::entity parent) const {
        static const std::vector<entt::entity> empty;
        auto                                   it = byParent_.find(parent);
        return it != byParent_.end() ? it->second : empty;
    }

    bool ChildrenIndex::hasChildren(entt::entity parent) const {
        return byParent_.find(parent) != byParent_.end();
    }

    std::string entityDisplayName(const entt::registry& registry, entt::entity entity) {
        if (registry.all_of<NameComponent>(entity)) {
            return registry.get<NameComponent>(entity).name;
        }
        return "Object " + std::to_string(static_cast<uint32_t>(entity));
    }

    void drawEntityRow(entt::entity entity,
        const char*                 icon,
        ImVec4                      color,
        FrameInfo&                  frameInfo,
        const entt::registry&       registry,
        std::vector<entt::entity>&  toDelete) {
        const auto id = static_cast<uint32_t>(entity);
        assert(id <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
        ImGui::PushID(static_cast<int>(id));

        const std::string label      = entityDisplayName(registry, entity) + " " + std::to_string(id);
        const bool        isSelected = (frameInfo.selectedEntity == entity);

        UI::TextColored(icon, color);
        ImGui::SameLine();

        const ImGuiStyle& style              = ImGui::GetStyle();
        float             actionsWidth       = 0.0f;
        auto              actionWidthForText = [&](const char* text) {
            return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f);
        };
        const bool isCamera       = registry.all_of<CameraComponent>(entity);
        const bool isActiveCamera = (entity == frameInfo.cameraEntity);

        if (isCamera) {
            actionsWidth += isActiveCamera ? ImGui::CalcTextSize("Active").x
                                           : actionWidthForText("Set Active");
            actionsWidth += style.ItemSpacing.x;
        }
        actionsWidth += isActiveCamera ? ImGui::CalcTextSize("Delete").x : actionWidthForText("Delete");

        const float selectableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - actionsWidth);

        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_Text,
            isSelected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        const bool clicked = ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_None,
            ImVec2(selectableWidth, 0.0f));
        ImGui::PopStyleColor(4);

        if (clicked) {
            frameInfo.selectedObjectId = id;
            frameInfo.selectedEntity   = entity;
        }

        ImGui::SameLine(0.0f, 0.0f);
        if (isCamera) {
            if (isActiveCamera) {
                UI::TextDisabled("Active");
                ImGui::SameLine(0.0f, style.ItemSpacing.x);
            } else {
                const std::string activeBtn = "Set Active##cam_" + std::to_string(id);
                if (UI::SmallButton(activeBtn.c_str())) {
                    frameInfo.cameraEntity = entity;
                }
                ImGui::SameLine(0.0f, style.ItemSpacing.x);
            }
        }

        if (isActiveCamera) {
            UI::TextDisabled("Delete");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Cannot delete the active camera");
            }
        } else {
            const std::string delBtn = "Delete##del_" + std::to_string(id);
            if (UI::SmallButton(delBtn.c_str())) {
                toDelete.push_back(entity);
            }
        }
        ImGui::PopID();
    }

    void drawInstancesFor(entt::entity modelEntity,
        uint32_t                       subMeshIndex,
        const ChildrenIndex&           childrenIndex,
        const entt::registry&          registry,
        FrameInfo&                     frameInfo,
        std::vector<entt::entity>&     toDelete) {
        const auto*  modelComp = registry.try_get<ModelComponent>(modelEntity);
        const Model* model     = modelComp ? modelComp->model.get() : nullptr;
        if (model == nullptr || subMeshIndex >= model->getSubMeshes().size()) {
            return;
        }
        const int nodeIndex = model->getSubMeshes()[subMeshIndex].nodeIndex;
        if (nodeIndex < 0) {
            return;  // sub-mesh not bound to a glTF node
        }
        // Find the node entity with this node index (child of the model).
        entt::entity instanceEntity = entt::null;
        for (auto child : childrenIndex.childrenOf(modelEntity)) {
            if (registry.all_of<NodeIndexComponent>(child) &&
                registry.get<NodeIndexComponent>(child).nodeIndex == nodeIndex) {
                instanceEntity = child;
                break;
            }
        }
        if (instanceEntity == entt::null) {
            return;  // node entity not present (e.g. skinned-only)
        }
        const char*  icon  = ICON_FA_CLONE;
        const ImVec4 color = Theme::Instance;
        drawEntityRow(instanceEntity, icon, color, frameInfo, registry, toDelete);
    }

    void drawSubMeshChildren(entt::entity parent,
        const ChildrenIndex&              childrenIndex,
        const entt::registry&             registry,
        FrameInfo&                        frameInfo,
        std::vector<entt::entity>&        toDelete) {
        for (auto child : childrenIndex.childrenOf(parent)) {
            if (!registry.all_of<SubMeshComponent>(child)) {
                continue;
            }
            const std::string name         = entityDisplayName(registry, child);
            const char*       icon         = ICON_FA_SHAPES;
            const ImVec4      color        = Theme::SubMesh;
            const uint32_t    subMeshIndex = registry.get<SubMeshComponent>(child).subMeshIndex;

            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(child)));
            UI::TextColored(icon, color);
            ImGui::SameLine();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
            if (frameInfo.selectedEntity == child) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            const bool open = ImGui::TreeNodeEx(name.c_str(), flags);
            if (ImGui::IsItemClicked()) {
                frameInfo.selectedObjectId = static_cast<uint32_t>(child);
                frameInfo.selectedEntity   = child;
            }
            if (open) {
                drawInstancesFor(parent, subMeshIndex, childrenIndex, registry, frameInfo, toDelete);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void drawLightChildren(entt::entity parent,
        const ChildrenIndex&            childrenIndex,
        const entt::registry&           registry,
        FrameInfo&                      frameInfo,
        std::vector<entt::entity>&      toDelete) {
        for (auto child : childrenIndex.childrenOf(parent)) {
            const char* icon  = ICON_FA_CIRCLE;
            ImVec4      color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            if (registry.all_of<PointLightComponent>(child)) {
                icon  = ICON_FA_BULLSEYE;
                color = Theme::Light;
            } else if (registry.all_of<DirectionalLightComponent>(child)) {
                icon  = ICON_FA_SUN;
                color = Theme::Light;
            } else if (registry.all_of<SpotLightComponent>(child)) {
                icon  = ICON_FA_LOCATION_ARROW;
                color = Theme::Light;
            } else {
                continue;  // not a light
            }
            drawEntityRow(child, icon, color, frameInfo, registry, toDelete);
        }
    }

}  // namespace engine::ui
