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
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine::ui {

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

            std::string label = "Object " + std::to_string(id);
            if (registry.all_of<NameComponent>(entity)) {
                label = registry.get<NameComponent>(entity).name + " " + std::to_string(id);
            }

            const bool isSelected = (frameInfo.selectedEntity == entity);

            UI::TextColored(icon, color);
            ImGui::SameLine();

            const ImGuiStyle& style        = ImGui::GetStyle();
            float             actionsWidth = 0.0f;

            auto actionWidthForText = [&](const char* text) {
                return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f);
            };

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

            if (registry.all_of<CameraComponent>(entity)) {
                if (entity == frameInfo.cameraEntity) {
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

            if (entity == frameInfo.cameraEntity) {
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
        (void) filter;
        (void) toDelete;

        const std::string header = "Cameras (" + std::to_string(cameras.size()) + ")";
        ImGui::PushID("cameras_header");
        const ImGuiStyle& style = ImGui::GetStyle();

        const float btnW = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();

        const bool open = UI::TreeNode(ICON_FA_CAMERA, header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

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
        (void) filter;

        const size_t      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
        const std::string header      = "Lights (" + std::to_string(lightsTotal) + ")";
        ImGui::PushID("lights_header");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();
        const bool open = UI::TreeNode(ICON_FA_LIGHTBULB, header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (UI::SmallButton("+##add_light")) {
            ImGui::OpenPopup("AddLightPopup");
        }
        if (ImGui::BeginPopup("AddLightPopup")) {
            const bool canAddDirectional = dirLights.empty();
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
            const std::string dirHeader = "Directional (" + std::to_string(dirLights.size()) + ")";
            if (UI::TreeNode(ICON_FA_SUN, dirHeader.c_str())) {
                for (auto entity : dirLights) {
                    drawEntityRow(entity, ICON_FA_SUN, ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry, toDelete);
                }
                ImGui::TreePop();
            }
            const std::string pntHeader = "Point (" + std::to_string(pointLights.size()) + ")";
            if (UI::TreeNode(ICON_FA_BULLSEYE, pntHeader.c_str())) {
                for (auto entity : pointLights) {
                    drawEntityRow(entity, ICON_FA_BULLSEYE, ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry, toDelete);
                }
                ImGui::TreePop();
            }
            const std::string sptHeader = "Spot (" + std::to_string(spotLights.size()) + ")";
            if (UI::TreeNode(ICON_FA_LOCATION_ARROW, sptHeader.c_str())) {
                for (auto entity : spotLights) {
                    drawEntityRow(entity, ICON_FA_LOCATION_ARROW, ImVec4(1.0f, 1.0f, 0.0f, 1.0f), frameInfo, registry, toDelete);
                }
                ImGui::TreePop();
            }
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
        (void) filter;
        (void) scene;

        const std::string header = "Models (" + std::to_string(models.size()) + ")";
        ImGui::PushID("models_header");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        ImGui::SetNextItemAllowOverlap();
        const bool open = UI::TreeNode(ICON_FA_CUBE, header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
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
                const std::string autoText = "Auto tokens: col_, ucx_, collision, collider, wall, floor, ground, world, level, static";
                UI::TextDisabled(autoText.c_str());
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

            const std::string indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
            try {
                std::ifstream f(indexPath);
                if (!f.is_open()) {
                    const std::string errText = "Failed to open model index: " + indexPath;
                    UI::TextDisabled(errText.c_str());
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

                        if (filterModel[0] != '\0') {
                            std::string lowName   = name;
                            std::string lowFilter = filterModel;
                            std::transform(lowName.begin(), lowName.end(), lowName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            std::transform(lowFilter.begin(), lowFilter.end(), lowFilter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if (lowName.find(lowFilter) == std::string::npos) {
                                continue;
                            }
                        }

                        if (UI::Selectable(name.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups)) {
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
            } catch (const std::exception& e) {
                const std::string errText = "Error parsing model index: " + std::string(e.what());
                UI::TextDisabled(errText.c_str());
            }

            ImGui::EndPopup();
        }
        ImGui::PopID();
        if (open) {
            for (auto entity : models) {
                drawEntityRow(entity, ICON_FA_CUBE, ImVec4(0.4f, 0.8f, 1.0f, 1.0f), frameInfo, registry, toDelete);
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

        const std::string loadText = "Pending loads: " + std::to_string(pendingLoads.size());
        UI::TextDisabled(loadText.c_str());
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
