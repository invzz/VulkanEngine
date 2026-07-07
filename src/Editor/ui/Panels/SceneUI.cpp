#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/ChildComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/NodeIndexComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine::ui {
    namespace {

        // ---------------------------------------------------------------
        // Small string helpers
        // ---------------------------------------------------------------

        std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        /// True if `filter` is empty, or `name` contains it (case-insensitive).
        bool matchesFilter(const std::string& name, const char* filter) {
            if (filter == nullptr || filter[0] == '\0') {
                return true;
            }
            return toLower(name).find(toLower(filter)) != std::string::npos;
        }

        std::string entityDisplayName(const entt::registry& registry, entt::entity entity) {
            if (registry.all_of<NameComponent>(entity)) {
                return registry.get<NameComponent>(entity).name;
            }
            return "Object " + std::to_string(static_cast<uint32_t>(entity));
        }

        // ---------------------------------------------------------------
        // Static-collider auto-detection tokens.
        // This is the single source of truth: both the detection logic
        // and the UI tooltip text are derived from it, so they can't
        // drift apart the way the hand-typed tooltip string used to.
        // ---------------------------------------------------------------

        const std::vector<std::string>& staticColliderAutoTokens() {
            static const std::vector<std::string> tokens = {
                "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};
            return tokens;
        }

        std::string staticColliderAutoTokensTooltip() {
            std::string text = "Auto tokens: ";
            const auto& tokens = staticColliderAutoTokens();
            for (size_t i = 0; i < tokens.size(); ++i) {
                text += tokens[i];
                if (i + 1 < tokens.size()) {
                    text += ", ";
                }
            }
            return text;
        }

        bool shouldAutoCreateStaticCollider(const std::string& path, const std::string& name) {
            const std::string combined = toLower(path + " " + name);
            for (const auto& token : staticColliderAutoTokens()) {
                if (combined.find(token) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

        bool isRootEntity(const entt::registry& registry, entt::entity entity) {
            return !registry.all_of<ChildComponent>(entity);
        }

        // ---------------------------------------------------------------
        // Parent -> children index.
        //
        // Building this once per section draw (instead of re-scanning the
        // whole ChildComponent view for every node, and again for every
        // node's "does it have children" check) turns what was an O(n^2)
        // hierarchy walk into O(n).
        // ---------------------------------------------------------------

        class ChildrenIndex {
        public:
            explicit ChildrenIndex(const entt::registry& registry) {
                auto view = registry.view<ChildComponent>();
                for (auto child : view) {
                    const auto parent = registry.get<ChildComponent>(child).parent;
                    byParent_[parent].push_back(child);
                }
            }

            const std::vector<entt::entity>& childrenOf(entt::entity parent) const {
                static const std::vector<entt::entity> empty;
                auto it = byParent_.find(parent);
                return it != byParent_.end() ? it->second : empty;
            }

            bool hasChildren(entt::entity parent) const {
                return byParent_.find(parent) != byParent_.end();
            }

        private:
            std::unordered_map<entt::entity, std::vector<entt::entity>> byParent_;
        };

        // ---------------------------------------------------------------
        // Entity creation helper — collapses the repeated
        // "Transform + Component + Name" triple used for every
        // camera/light type into one call.
        // ---------------------------------------------------------------

        template <typename Component>
        entt::entity createNamedEntity(engine::Scene& scene, entt::registry& registry, const char* name) {
            auto entity = scene.createEntity();
            registry.emplace<TransformComponent>(entity);
            registry.emplace<Component>(entity);
            registry.emplace<NameComponent>(entity, name);
            return entity;
        }

        // ---------------------------------------------------------------
        // Section header with a trailing "+" add button, right-aligned.
        // Shared by the Cameras / Lights / Models sections, which used
        // to duplicate this PushID/TreeNode/SameLine/SmallButton/PopID
        // dance three times.
        // ---------------------------------------------------------------

        struct SectionHeaderResult {
            bool open;
            bool addClicked;
        };

        SectionHeaderResult drawSectionHeaderWithAddButton(
            const char*        pushIdLabel,
            const char*        icon,
            const std::string& header,
            const char*        addButtonId,
            ImGuiTreeNodeFlags flags = 0) {
            ImGui::PushID(pushIdLabel);
            const ImGuiStyle& style = ImGui::GetStyle();
            const float       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
            ImGui::SetNextItemAllowOverlap();
            const bool open = UI::TreeNode(icon, header.c_str(), flags);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
            const bool addClicked = UI::SmallButton(addButtonId);
            ImGui::PopID();
            return {open, addClicked};
        }

        // ---------------------------------------------------------------
        // Row / tree drawing
        // ---------------------------------------------------------------

        void drawEntityRow(
            entt::entity               entity,
            const char*                icon,
            ImVec4                     color,
            FrameInfo&                 frameInfo,
            const entt::registry&      registry,
            std::vector<entt::entity>& toDelete) {
            const auto id = static_cast<uint32_t>(entity);
            assert(id <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
            ImGui::PushID(static_cast<int>(id));

            const std::string label     = entityDisplayName(registry, entity) + " " + std::to_string(id);
            const bool         isSelected = (frameInfo.selectedEntity == entity);

            UI::TextColored(icon, color);
            ImGui::SameLine();

            const ImGuiStyle& style              = ImGui::GetStyle();
            float             actionsWidth       = 0.0f;
            auto              actionWidthForText = [&](const char* text) {
                return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f);
            };
            const bool isCamera      = registry.all_of<CameraComponent>(entity);
            const bool isActiveCamera = (entity == frameInfo.cameraEntity);

            if (isCamera) {
                actionsWidth += isActiveCamera ? ImGui::CalcTextSize("Active").x : actionWidthForText("Set Active");
                actionsWidth += style.ItemSpacing.x;
            }
            actionsWidth += isActiveCamera ? ImGui::CalcTextSize("Delete").x : actionWidthForText("Delete");

            const float selectableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - actionsWidth);

            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_Text, isSelected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            const bool clicked = ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(selectableWidth, 0.0f));
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

        /// Recursively draw node children of a parent entity using a
        /// pre-built children index (no per-call full-view scan).
        void drawNodeChildren(
            entt::entity               parent,
            const ChildrenIndex&       childrenIndex,
            const entt::registry&      registry,
            FrameInfo&                 frameInfo,
            std::vector<entt::entity>& toDelete) {
            for (auto child : childrenIndex.childrenOf(parent)) {
                if (!registry.all_of<NodeIndexComponent>(child)) {
                    continue;
                }

                const std::string name = entityDisplayName(registry, child);
                const char*       icon  = ICON_FA_FOLDER;
                const ImVec4      color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
                const bool        hasOwnChildren = childrenIndex.hasChildren(child);

                ImGui::PushID(static_cast<int>(static_cast<uint32_t>(child)));
                UI::TextColored(icon, color);
                ImGui::SameLine();

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                if (!hasOwnChildren) {
                    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                }
                if (frameInfo.selectedEntity == child) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool nodeOpen = ImGui::TreeNodeEx(name.c_str(), flags);
                if (ImGui::IsItemClicked()) {
                    frameInfo.selectedObjectId = static_cast<uint32_t>(child);
                    frameInfo.selectedEntity   = child;
                }
                if (hasOwnChildren && nodeOpen) {
                    drawNodeChildren(child, childrenIndex, registry, frameInfo, toDelete);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        /// Draw light children of a parent entity as flat selectable rows.
        void drawLightChildren(
            entt::entity               parent,
            const ChildrenIndex&       childrenIndex,
            const entt::registry&      registry,
            FrameInfo&                 frameInfo,
            std::vector<entt::entity>& toDelete) {
            for (auto child : childrenIndex.childrenOf(parent)) {
                const char* icon  = ICON_FA_CIRCLE;
                ImVec4      color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                if (registry.all_of<PointLightComponent>(child)) {
                    icon  = ICON_FA_BULLSEYE;
                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                } else if (registry.all_of<DirectionalLightComponent>(child)) {
                    icon  = ICON_FA_SUN;
                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                } else if (registry.all_of<SpotLightComponent>(child)) {
                    icon  = ICON_FA_LOCATION_ARROW;
                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                } else {
                    continue;  // not a light
                }
                drawEntityRow(child, icon, color, frameInfo, registry, toDelete);
            }
        }

    }  // namespace

    SceneEntityCollection UI::CollectSceneEntities(const engine::Scene& scene) {
        SceneEntityCollection result;
        auto&                 registry = scene.getRegistry();
        auto                  view     = registry.view<entt::entity>();
        result.cameras.reserve(view.size());
        result.dirLights.reserve(view.size());
        result.pointLights.reserve(view.size());
        result.spotLights.reserve(view.size());
        result.models.reserve(view.size());
        for (auto entity : view) {
            // Skip non-root entities (children of a parent)
            if (!isRootEntity(registry, entity)) {
                continue;
            }
            if (registry.all_of<CameraComponent>(entity)) {
                result.cameras.push_back(entity);
                continue;
            }
            if (registry.all_of<DirectionalLightComponent>(entity)) {
                result.dirLights.push_back(entity);
                continue;
            }
            if (registry.all_of<PointLightComponent>(entity)) {
                result.pointLights.push_back(entity);
                continue;
            }
            if (registry.all_of<SpotLightComponent>(entity)) {
                result.spotLights.push_back(entity);
                continue;
            }
            if (registry.all_of<ModelComponent>(entity)) {
                result.models.push_back(entity);
                continue;
            }
        }
        return result;
    }

    void UI::EnforceSingleDirectionalLight(
        std::vector<entt::entity>& dirLights,
        std::vector<entt::entity>& toDelete) {
        if (dirLights.size() > 1) {
            for (size_t i = 1; i < dirLights.size(); ++i) {
                const entt::entity extra = dirLights[i];
                if (std::find(toDelete.begin(), toDelete.end(), extra) == toDelete.end()) {
                    toDelete.push_back(extra);
                }
            }
            dirLights.resize(1);
        }
    }

    void UI::DrawSceneCameraSection(
        const std::vector<entt::entity>& cameras,
        const char*                      filter,
        FrameInfo&                       frameInfo,
        engine::Scene&                   scene,
        entt::registry&                  registry,
        std::vector<entt::entity>&       toDelete) {
        const std::string header = "Cameras (" + std::to_string(cameras.size()) + ")";
        const auto result = drawSectionHeaderWithAddButton(
            "cameras_header", ICON_FA_CAMERA, header, "+##add_camera", ImGuiTreeNodeFlags_DefaultOpen);

        if (result.addClicked) {
            createNamedEntity<CameraComponent>(scene, registry, "Camera");
        }

        if (result.open) {
            for (auto entity : cameras) {
                if (!matchesFilter(entityDisplayName(registry, entity), filter)) {
                    continue;
                }
                drawEntityRow(entity, "[CAM]", ImVec4(1.0f, 1.0f, 1.0f, 1.0f), frameInfo, registry, toDelete);
            }
            ImGui::TreePop();
        }
    }

    void UI::DrawSceneLightSection(
        const std::vector<entt::entity>& dirLights,
        const std::vector<entt::entity>& pointLights,
        const std::vector<entt::entity>& spotLights,
        const char*                      filter,
        FrameInfo&                       frameInfo,
        engine::Scene&                   scene,
        entt::registry&                  registry,
        std::vector<entt::entity>&       toDelete) {
        const size_t      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
        const std::string header      = "Lights (" + std::to_string(lightsTotal) + ")";
        const auto result = drawSectionHeaderWithAddButton(
            "lights_header", ICON_FA_LIGHTBULB, header, "+##add_light", ImGuiTreeNodeFlags_DefaultOpen);

        if (result.addClicked) {
            ImGui::OpenPopup("AddLightPopup");
        }

        if (ImGui::BeginPopup("AddLightPopup")) {
            const bool canAddDirectional = dirLights.empty();
            if (!canAddDirectional) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable("Add Directional")) {
                if (canAddDirectional) {
                    createNamedEntity<DirectionalLightComponent>(scene, registry, "Directional Light");
                }
                ImGui::CloseCurrentPopup();
            }
            if (!canAddDirectional) {
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Only one directional light is allowed.");
                }
                ImGui::EndDisabled();
            }
            if (ImGui::Selectable("Add Point")) {
                createNamedEntity<PointLightComponent>(scene, registry, "Point Light");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Add Spot")) {
                createNamedEntity<SpotLightComponent>(scene, registry, "Spot Light");
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (result.open) {
            auto drawGroup = [&](const char* icon, const char* label, const std::vector<entt::entity>& group) {
                const std::string groupHeader = std::string(label) + " (" + std::to_string(group.size()) + ")";
                if (UI::TreeNode(icon, groupHeader.c_str())) {
                    for (auto entity : group) {
                        if (!matchesFilter(entityDisplayName(registry, entity), filter)) {
                            continue;
                        }
                        drawEntityRow(entity, icon, ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry, toDelete);
                    }
                    ImGui::TreePop();
                }
            };
            drawGroup(ICON_FA_SUN, "Directional", dirLights);
            drawGroup(ICON_FA_BULLSEYE, "Point", pointLights);
            drawGroup(ICON_FA_LOCATION_ARROW, "Spot", spotLights);
            ImGui::TreePop();
        }
    }

    void UI::DrawSceneModelSection(
        const std::vector<entt::entity>&                 models,
        const char*                                      filter,
        FrameInfo&                                       frameInfo,
        engine::Scene&                                   scene,
        entt::registry&                                  registry,
        std::vector<entt::entity>&                       toDelete,
        ModelInsertionOptions::StaticColliderImportMode& colliderMode,
        std::function<void(
            const std::string&,
            const std::string&,
            const ModelInsertionOptions&,
            ModelInsertionOptions::StaticColliderImportMode)>
            enqueueModelLoad) {
        (void) scene;

        const std::string header = "Models (" + std::to_string(models.size()) + ")";
        const auto result = drawSectionHeaderWithAddButton(
            "models_header", ICON_FA_CUBE, header, "+##add_model", ImGuiTreeNodeFlags_DefaultOpen);

        if (result.addClicked) {
            ImGui::OpenPopup("AddModelPopup");
        }

        if (ImGui::BeginPopup("AddModelPopup")) {
            static char filterModel[128] = "";
            UI::InputText("Filter", filterModel, sizeof(filterModel));

            int                colliderModeIndex = static_cast<int>(colliderMode);
            static const char* modeLabels[]      = {"Auto Detect", "Force On", "Force Off"};
            if (UI::Combo("Static Mesh Collider", &colliderModeIndex, modeLabels, 3)) {
                colliderMode = static_cast<ModelInsertionOptions::StaticColliderImportMode>(colliderModeIndex);
            }
            if (colliderMode == ModelInsertionOptions::StaticColliderImportMode::AutoDetect) {
                UI::TextDisabled(staticColliderAutoTokensTooltip().c_str());
            }
            UI::Separator();

            static char customPath[512] = "";
            UI::InputText("Path (.gltf/.glb)", customPath, sizeof(customPath));
            ImGui::SameLine();
            if (UI::SmallButton("Load Path")) {
                if (customPath[0] != '\0') {
                    std::filesystem::path p(customPath);
                    if (std::filesystem::exists(p)) {
                        std::string ext = p.extension().string();
                        if (ext == ".gltf" || ext == ".glb") {
                            std::string           name = p.stem().string();
                            ModelInsertionOptions opts;
                            opts.enableTextures     = true;
                            opts.loadMaterials      = true;
                            opts.enableMorphTargets = true;
                            enqueueModelLoad(customPath, name, opts, colliderMode);
                            customPath[0] = '\0';
                            ImGui::CloseCurrentPopup();
                        } else {
                            UI::TextDisabled(("Unsupported format: " + std::string(ext)).c_str());
                        }
                    } else {
                        UI::TextDisabled(("File not found: " + std::string(customPath)).c_str());
                    }
                }
            }

            const std::string indexPath = std::string(MODEL_PATH) + "glTF/model-index.json";
            try {
                std::ifstream f(indexPath);
                if (!f.is_open()) {
                    UI::TextDisabled(("Failed to open model index: " + indexPath).c_str());
                } else {
                    nlohmann::json j;
                    f >> j;
                    for (const auto& item : j) {
                        if (!item.contains("name")) {
                            continue;
                        }
                        const std::string name = item["name"].get<std::string>();

                        std::string relativePath;
                        if (item.contains("variants") && item["variants"].contains("glTF")) {
                            const std::string variantFile = item["variants"]["glTF"].get<std::string>();
                            relativePath.append("glTF/").append(name).append("/glTF/").append(variantFile);
                        }

                        if (!matchesFilter(name, filterModel)) {
                            continue;
                        }

                        if (UI::Selectable(name.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups)) {
                            std::string fullPath;
                            if (relativePath.empty()) {
                                fullPath.append(MODEL_PATH).append("glTF/").append(name).append("/glTF/").append(name).append(".gltf");
                            } else {
                                fullPath.append(MODEL_PATH).append(relativePath);
                            }
                            ModelInsertionOptions opts;
                            opts.enableTextures     = true;
                            opts.loadMaterials      = true;
                            opts.enableMorphTargets = true;
                            enqueueModelLoad(fullPath, name, opts, colliderMode);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            } catch (const std::exception& e) {
                UI::TextDisabled(("Error parsing model index: " + std::string(e.what())).c_str());
            }
            ImGui::EndPopup();
        }

        if (result.open) {
            // Built once for the whole model list instead of per-node.
            const ChildrenIndex childrenIndex(registry);

            for (auto entity : models) {
                if (!matchesFilter(entityDisplayName(registry, entity), filter)) {
                    continue;
                }

                drawEntityRow(entity, ICON_FA_CUBE, ImVec4(0.4f, 0.8f, 1.0f, 1.0f), frameInfo, registry, toDelete);

                bool hasLights = false;
                bool hasNodes  = false;
                for (auto child : childrenIndex.childrenOf(entity)) {
                    if (registry.all_of<PointLightComponent>(child) ||
                        registry.all_of<DirectionalLightComponent>(child) ||
                        registry.all_of<SpotLightComponent>(child)) {
                        hasLights = true;
                    } else if (registry.all_of<NodeIndexComponent>(child)) {
                        hasNodes = true;
                    }
                }

                if (hasLights) {
                    if (UI::TreeNode(ICON_FA_LIGHTBULB, "Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
                        drawLightChildren(entity, childrenIndex, registry, frameInfo, toDelete);
                        ImGui::TreePop();
                    }
                }
                if (hasNodes) {
                    if (UI::TreeNode(ICON_FA_SITEMAP, "Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
                        drawNodeChildren(entity, childrenIndex, registry, frameInfo, toDelete);
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    void UI::DrawScenePendingLoadsSection(
        std::vector<ScenePendingModelLoad>& pendingLoads,
        ResourceManager*                    resourceManager) {
        if (pendingLoads.empty()) {
            return;
        }
        std::unordered_map<AsyncLoadId, AsyncLoadSnapshot> snapshotById;
        if (resourceManager != nullptr) {
            const auto snapshots = resourceManager->getAsyncLoadSnapshots();
            for (const auto& snapshot : snapshots) {
                snapshotById[snapshot.id] = snapshot;
            }
        }
        UI::TextDisabled(("Pending loads: " + std::to_string(pendingLoads.size())).c_str());
        for (size_t i = 0; i < pendingLoads.size(); ++i) {
            auto& p = pendingLoads[i];
            UI::TextDisabled(p.name.c_str());
            ImGui::SameLine();
            auto snapshotIt = snapshotById.find(p.id);
            if (snapshotIt != snapshotById.end()) {
                const auto& snapshot = snapshotIt->second;
                UI::ProgressBar(snapshot.progress, ImVec2(120.0f, 0.0f));
                ImGui::SameLine();
                switch (snapshot.status) {
                    case LoadStatus::PENDING:
                        UI::TextDisabled("Pending");
                        break;
                    case LoadStatus::LOADING:
                        UI::TextDisabled("Loading");
                        break;
                    case LoadStatus::COMPLETE:
                        UI::TextDisabled("Done");
                        break;
                    case LoadStatus::FAILED:
                        UI::TextDisabled("Failed");
                        break;
                }
                ImGui::SameLine();
            }
            const std::string cancelBtn = "Cancel##" + std::to_string(i);
            if (UI::SmallButton(cancelBtn.c_str())) {
                p.cancelled = true;
                if (resourceManager != nullptr) {
                    resourceManager->cancelModelLoad(p.id);
                }
            }
        }
        UI::Separator();
    }

    bool UI::ShouldCreateStaticCollider(
        const std::string&                              path,
        const std::string&                              name,
        ModelInsertionOptions::StaticColliderImportMode mode) {
        switch (mode) {
            case ModelInsertionOptions::StaticColliderImportMode::ForceOn:
                return true;
            case ModelInsertionOptions::StaticColliderImportMode::ForceOff:
                return false;
            case ModelInsertionOptions::StaticColliderImportMode::AutoDetect:
            default:
                return shouldAutoCreateStaticCollider(path, name);
        }
    }

}  // namespace engine::ui