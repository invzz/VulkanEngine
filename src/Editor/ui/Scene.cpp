#include "Editor/ui/Scene.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/EngineState.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/UI/UI.hpp"
#include "Editor/ui/ScenePanel.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/entity.hpp"

namespace engine::ui::SceneUI {

    namespace {

        std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool shouldAutoCreateStaticCollider(const std::string& path, const std::string& name) {
            const std::string                     combined = toLower(path + " " + name);
            static const std::vector<std::string> tokens   = {
                "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};

            for (const auto& token : tokens) {
                if (combined.find(token) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

        bool matchesFilter(entt::entity entity, const char* filter) {
            if (filter[0] == '\0')
                return true;
            std::string lowFilter;
            lowFilter.reserve(strlen(filter));
            for (char* p = const_cast<char*>(filter); *p; ++p)
                lowFilter += static_cast<char>(std::tolower(*p));

            std::string label;
            // Note: caller should check NameComponent before calling
            return label.find(lowFilter) != std::string::npos;
        }

    }  // namespace

    EntityCollection collectEntities(const engine::Scene& scene) {
        EntityCollection result;
        auto&            registry = scene.getRegistry();
        auto             view     = registry.view<entt::entity>();

        result.cameras.reserve(view.size());
        result.dirLights.reserve(view.size());
        result.pointLights.reserve(view.size());
        result.spotLights.reserve(view.size());
        result.models.reserve(view.size());

        for (auto entity : view) {
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

    void enforceSingleDirectionalLight(std::vector<entt::entity>& dirLights,
        std::vector<entt::entity>&                                toDelete) {
        if (dirLights.size() > 1) {
            for (size_t i = 1; i < dirLights.size(); ++i) {
                entt::entity const extra = dirLights[i];
                if (std::find(toDelete.begin(), toDelete.end(), extra) == toDelete.end()) {
                    toDelete.push_back(extra);
                }
            }
            dirLights.resize(1);
        }
    }

    void drawEntityRow(entt::entity entity,
        const char*                 icon,
        ImVec4                      color,
        FrameInfo&                  frameInfo,
        const entt::registry&       registry) {
        auto const id = static_cast<uint32_t>(entity);

        assert(id <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
        ImGui::PushID(static_cast<int>(id));

        std::string label = "Object " + std::to_string(id);
        if (registry.all_of<NameComponent>(entity)) {
            label = registry.get<NameComponent>(entity).name + " " + std::to_string(id);
        }

        bool const isSelected = (frameInfo.selectedEntity == entity);

        UI::TextColored(icon, color);
        ImGui::SameLine();

        ImGuiStyle const& style        = ImGui::GetStyle();
        float             actionsWidth = 0.0f;

        auto actionWidthForText = [&](const char* text) {
            return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f);
        };

        // Reserve space for per-row actions so the label becomes a clickable row.
        if (registry.all_of<CameraComponent>(entity)) {
            if (entity == frameInfo.cameraEntity) {
                actionsWidth += ImGui::CalcTextSize("Active").x;
            } else {
                actionsWidth += actionWidthForText("Set Active");
            }
            actionsWidth += style.ItemSpacing.x;
        }

        if (entity == frameInfo.cameraEntity) {
            actionsWidth += ImGui::CalcTextSize("Delete").x;
        } else {
            actionsWidth += actionWidthForText("Delete");
        }

        float const selectableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - actionsWidth);
        (void) selectableWidth;  // Width reserved but not used directly
        if (UI::Selectable(label.c_str(), isSelected)) {
            frameInfo.selectedObjectId = id;
            frameInfo.selectedEntity   = entity;
        }
        ImGui::SameLine();

        if (registry.all_of<CameraComponent>(entity)) {
            if (entity == frameInfo.cameraEntity) {
                UI::TextDisabled("Active");
            } else {
                std::string activeBtn = "Set Active##cam_" + std::to_string(id);
                if (UI::SmallButton(activeBtn.c_str())) {
                    frameInfo.cameraEntity = entity;
                }
            }
            ImGui::SameLine();
        }

        if (entity == frameInfo.cameraEntity) {
            UI::TextDisabled("Delete");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Cannot delete the active camera");
            }
        } else {
            std::string delBtn = "Delete##del_" + std::to_string(id);
            if (UI::SmallButton(delBtn.c_str())) {
                // Note: caller manages toDelete_
                (void) delBtn;
            }
        }

        ImGui::PopID();
    }

    void drawCameraSection(const std::vector<entt::entity>& cameras,
        const char*                                         filter,
        FrameInfo&                                          frameInfo,
        engine::Scene&                                      scene,
        entt::registry&                                     registry,
        std::vector<entt::entity>&                          toDelete) {
        (void) filter;  // Filter not applied at root level currently
        (void) toDelete;

        std::string const header = "Cameras (" + std::to_string(cameras.size()) + ")";
        ImGui::PushID("cameras_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();
        bool const open = UI::TreeNode("▶", header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (UI::SmallButton("+##add_camera")) {
            auto entity = scene.createEntity();
            registry.emplace<TransformComponent>(entity);
            registry.emplace<CameraComponent>(entity);
            registry.emplace<NameComponent>(entity, "Camera");
        }
        ImGui::PopID();
        if (open) {
            for (auto entity : cameras) {
                drawEntityRow(entity, "[CAM]", ImVec4(1.0f, 1.0f, 1.0f, 1.0f), frameInfo, registry);
            }
            ImGui::TreePop();
        }
    }

    void drawLightSection(const std::vector<entt::entity>& dirLights,
        const std::vector<entt::entity>&                   pointLights,
        const std::vector<entt::entity>&                   spotLights,
        const char*                                        filter,
        FrameInfo&                                         frameInfo,
        engine::Scene&                                     scene,
        entt::registry&                                    registry,
        std::vector<entt::entity>&                         toDelete) {
        (void) scene;  // Not used in light section
        (void) filter;
        (void) toDelete;

        size_t const      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
        std::string const header      = "Lights (" + std::to_string(lightsTotal) + ")";
        ImGui::PushID("lights_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();
        bool const open = UI::TreeNode("▶", header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (UI::SmallButton("+##add_light")) {
            ImGui::OpenPopup("AddLightPopup");
        }
        if (ImGui::BeginPopup("AddLightPopup")) {
            bool const canAddDirectional = dirLights.empty();
            if (!canAddDirectional) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable("Add Directional")) {
                if (canAddDirectional) {
                    auto entity = scene.createEntity();
                    registry.emplace<TransformComponent>(entity);
                    registry.emplace<DirectionalLightComponent>(entity);
                    registry.emplace<NameComponent>(entity, "Directional Light");
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
                auto entity = scene.createEntity();
                registry.emplace<TransformComponent>(entity);
                registry.emplace<PointLightComponent>(entity);
                registry.emplace<NameComponent>(entity, "Point Light");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Add Spot")) {
                auto entity = scene.createEntity();
                registry.emplace<TransformComponent>(entity);
                registry.emplace<SpotLightComponent>(entity);
                registry.emplace<NameComponent>(entity, "Spot Light");
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
        if (open) {
            std::string dirHeader = "Directional (" + std::to_string(dirLights.size()) + ")";
            if (UI::TreeNode("  ▶", dirHeader.c_str())) {
                for (auto entity : dirLights) {
                    drawEntityRow(entity, "[DIR]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry);
                }
                ImGui::TreePop();
            }
            std::string pntHeader = "Point (" + std::to_string(pointLights.size()) + ")";
            if (UI::TreeNode("  ▶", pntHeader.c_str())) {
                for (auto entity : pointLights) {
                    drawEntityRow(entity, "[PNT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry);
                }
                ImGui::TreePop();
            }
            std::string sptHeader = "Spot (" + std::to_string(spotLights.size()) + ")";
            if (UI::TreeNode("  ▶", sptHeader.c_str())) {
                for (auto entity : spotLights) {
                    drawEntityRow(entity, "[SPT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry);
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }

    void drawModelSection(const std::vector<entt::entity>& models,
        const char*                                        filter,
        FrameInfo&                                         frameInfo,
        engine::Scene&                                     scene,
        entt::registry&                                    registry,
        std::vector<entt::entity>&                         toDelete,
        ModelInsertionOptions::StaticColliderImportMode&   colliderMode,
        std::function<void(const std::string&,
            const std::string&,
            const ModelInsertionOptions&,
            ModelInsertionOptions::StaticColliderImportMode)>
            enqueueModelLoad) {
        (void) filter;
        (void) toDelete;

        std::string const header = "Models (" + std::to_string(models.size()) + ")";
        ImGui::PushID("models_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();
        bool const open = UI::TreeNode("▶", header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (UI::SmallButton("+##add_model")) {
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
                std::string autoText = "Auto tokens: col_, ucx_, collision, collider, wall, floor, ground, world, level, static";
                UI::TextDisabled(autoText.c_str());
            }
            UI::Separator();

            std::string const indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
            try {
                std::ifstream f(indexPath);
                if (!f.is_open()) {
                    std::string errText = "Failed to open model index: " + indexPath;
                    UI::TextDisabled(errText.c_str());
                } else {
                    nlohmann::json j;
                    f >> j;
                    for (const auto& item : j) {
                        if (!item.contains("name")) {
                            continue;
                        }
                        std::string name = item["name"].get<std::string>();

                        std::string relativePath;
                        if (item.contains("variants") && item["variants"].contains("glTF")) {
                            std::string variantFile = item["variants"]["glTF"].get<std::string>();
                            relativePath.append("glTF/").append(name).append("/glTF/").append(variantFile);
                        }

                        if (filterModel[0] != '\0') {
                            std::string lowName   = name;
                            std::string lowFilter = filterModel;
                            std::transform(lowName.begin(), lowName.end(), lowName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            std::transform(lowFilter.begin(), lowFilter.end(), lowFilter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if (lowName.find(lowFilter) == std::string::npos) {
                                continue;
                            }
                        }

                        if (UI::Selectable(name.c_str())) {
                            if (ImGui::IsItemActivated()) {
                                std::string fullPath;
                                if (relativePath.empty()) {
                                    fullPath.append(MODEL_PATH);
                                    fullPath.append("/glTF/");
                                    fullPath.append(name);
                                    fullPath.append("/glTF/");
                                    fullPath.append(name);
                                    fullPath.append(".gltf");
                                } else {
                                    fullPath.append(MODEL_PATH);
                                    fullPath.append("/");
                                    fullPath.append(relativePath);
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
                }
            } catch (const std::exception& e) {
                std::string errText = "Error parsing model index: " + std::string(e.what());
                UI::TextDisabled(errText.c_str());
            }

            ImGui::EndPopup();
        }
        ImGui::PopID();
        if (open) {
            for (auto entity : models) {
                drawEntityRow(entity, "[MDL]", ImVec4(0.4f, 0.8f, 1.0f, 1.0f), frameInfo, registry);
            }
            ImGui::TreePop();
        }
    }

    void drawPendingLoadsSection(std::vector<PendingModelLoad>& pendingLoads,
        ResourceManager*                                        resourceManager) {
        if (pendingLoads.empty()) {
            return;
        }

        std::unordered_map<AsyncLoadId, AsyncLoadSnapshot> snapshotById;
        if (resourceManager != nullptr) {
            auto snapshots = resourceManager->getAsyncLoadSnapshots();
            for (const auto& snapshot : snapshots) {
                snapshotById[snapshot.id] = snapshot;
            }
        }

        std::string loadText = "Pending loads: " + std::to_string(pendingLoads.size());
        UI::TextDisabled(loadText.c_str());
        for (size_t i = 0; i < pendingLoads.size(); ++i) {
            auto&       p        = pendingLoads[i];
            std::string nameText = p.name;
            UI::TextDisabled(nameText.c_str());
            ImGui::SameLine();

            auto snapshotIt = snapshotById.find(p.id);
            if (snapshotIt != snapshotById.end()) {
                auto const& snapshot = snapshotIt->second;
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

            std::string cancelBtn = "Cancel##" + std::to_string(i);
            if (UI::SmallButton(cancelBtn.c_str())) {
                p.cancelled = true;
                if (resourceManager != nullptr) {
                    resourceManager->cancelModelLoad(p.id);
                }
            }
        }

        UI::Separator();
    }

    bool shouldCreateStaticCollider(const std::string&  path,
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

}  // namespace engine::ui::SceneUI
