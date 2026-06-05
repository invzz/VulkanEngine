#include "Editor/ui/ScenePanel.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Editor/UI/UI.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool shouldAutoCreateStaticCollider(const std::string& path, const std::string& name) {
    const std::string combined = toLower(path + " " + name);
    static const std::vector<std::string> tokens = {
        "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};

    for (const auto& token : tokens) {
        if (combined.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool shouldCreateStaticCollider(const std::string& path, const std::string& name, ScenePanel::StaticColliderImportMode mode) {
    switch (mode) {
        case ScenePanel::StaticColliderImportMode::ForceOn:
            return true;
        case ScenePanel::StaticColliderImportMode::ForceOff:
            return false;
        case ScenePanel::StaticColliderImportMode::AutoDetect:
        default:
            return shouldAutoCreateStaticCollider(path, name);
    }
}
}  // namespace

    ScenePanel::ScenePanel(Device& device, EngineState* engineState)
        : device_(device), engineState_(engineState) {}

    void ScenePanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }

        if (engineState_ == nullptr) {
            return;
        }

        auto sceneState = engineState_->sceneRuntimeService().view();
        if (sceneState.scene == nullptr) {
            return;
        }

        // Push theme style
        ui::UI::PushThemeStyle();

        Scene& scene = *sceneState.scene;
        auto& registry = scene.getRegistry();
        auto resources = engineState_->resourceService().view();

        if (ImGui::Begin("Scene Objects", &visible_)) {
            if (resources.resourceManager != nullptr) {
                resources.resourceManager->updateAsyncCallbacks();
            }

            // --- Search filter ---
            static char searchFilter[128] = "";
            ImGui::SetNextItemWidth(-1);
            ui::UI::InputText("##search", searchFilter, sizeof(searchFilter));
            ui::UI::Separator();

            auto enqueueModelLoad = [&](const std::string& fullPath,
                                        const std::string& name,
                                        const engine::ModelInsertionOptions& opts,
                                        StaticColliderImportMode colliderMode) {
                if ((engineState_ == nullptr) || (resources.resourceManager == nullptr)) {
                    return;
                }

                AsyncLoadId const id = resources.resourceManager->enqueueModelLoad(
                    fullPath,
                    opts.enableTextures,
                    opts.loadMaterials,
                    opts.enableMorphTargets,
                    ResourcePriority::HIGH,
                    [this, fullPath, name, colliderMode](const std::shared_ptr<engine::Model>& modelPtr) {
                        if (!modelPtr || engineState_ == nullptr) {
                            std::cerr << "[Model] Async load returned null model: " << fullPath << "\n";
                            return;
                        }

                        auto sceneState = engineState_->sceneRuntimeService().view();
                        if (sceneState.scene == nullptr) {
                            std::cerr << "[Model] Scene runtime state unavailable for async insertion: " << fullPath << "\n";
                            return;
                        }

                        Scene& scene = *sceneState.scene;
                        auto& registry = scene.getRegistry();

                        auto entity = scene.createEntity();
                        registry.emplace<TransformComponent>(entity);
                        registry.emplace<ModelComponent>(entity, modelPtr);
                        registry.emplace<NameComponent>(entity, name);

                        if (shouldCreateStaticCollider(fullPath, name, colliderMode)) {
                            auto& rigidBody = registry.emplace<RigidBodyComponent>(entity);
                            rigidBody.isStatic = true;
                            rigidBody.mode = RigidBodyComponent::PhysicsMode::Static;
                            rigidBody.useGravity = false;

                            auto& collider = registry.emplace<ColliderComponent>(entity);
                            collider.shape = ColliderComponent::ShapeType::Mesh;
                            collider.isTrigger = false;
                        }

                        auto& modelComp = registry.get<ModelComponent>(entity);
                        if (modelComp.model->hasAnimations()) {
                            registry.emplace<AnimationComponent>(entity, modelComp.model);
                        }
                        if (modelComp.model->hasMorphTargets()) {
                            if (!registry.all_of<AnimationComponent>(entity)) {
                                registry.emplace<AnimationComponent>(entity, modelComp.model);
                            }
                        }

                        std::cout << "[Model] Added to scene (async): " << fullPath << "\n";
                    },
                    [fullPath](const std::string& error) {
                        std::cerr << "[Model] Async load failed for " << fullPath << ": " << error << '\n';
                    });

                PendingModelLoad pending;
                pending.id      = id;
                pending.path    = fullPath;
                pending.name    = name;
                pending.options = opts;
                pending.colliderMode = colliderMode;
                pendingLoads_.emplace_back(std::move(pending));
            };

            ui::UI::Separator();

            if (!pendingLoads_.empty()) {
                std::unordered_map<AsyncLoadId, AsyncLoadSnapshot> snapshotById;
                if ((engineState_ != nullptr) && (resources.resourceManager != nullptr)) {
                    auto snapshots = resources.resourceManager->getAsyncLoadSnapshots();
                    for (const auto& snapshot : snapshots) {
                        snapshotById[snapshot.id] = snapshot;
                    }
                }

                std::string loadText = "Pending loads: " + std::to_string(pendingLoads_.size());
                ui::UI::TextDisabled(loadText.c_str());
                for (size_t i = 0; i < pendingLoads_.size(); ++i) {
                    auto& p = pendingLoads_[i];
                    std::string nameText = p.name;
                    ui::UI::TextDisabled(nameText.c_str());
                    ImGui::SameLine();

                    auto snapshotIt = snapshotById.find(p.id);
                    if (snapshotIt != snapshotById.end()) {
                        auto const& snapshot = snapshotIt->second;
                        ui::UI::ProgressBar(snapshot.progress, ImVec2(120.0f, 0.0f));
                        ImGui::SameLine();
                        switch (snapshot.status) {
                            case LoadStatus::PENDING:
                                ui::UI::TextDisabled("Pending");
                                break;
                            case LoadStatus::LOADING:
                                ui::UI::TextDisabled("Loading");
                                break;
                            case LoadStatus::COMPLETE:
                                ui::UI::TextDisabled("Done");
                                break;
                            case LoadStatus::FAILED:
                                ui::UI::TextDisabled("Failed");
                                break;
                        }
                        ImGui::SameLine();
                    }

                    std::string cancelBtn = "Cancel##" + std::to_string(i);
                    if (ui::UI::SmallButton(cancelBtn.c_str())) {
                        p.cancelled = true;
                        if ((engineState_ != nullptr) && (resources.resourceManager != nullptr)) {
                            resources.resourceManager->cancelModelLoad(p.id);
                        }
                    }
                }

                ui::UI::Separator();
            }

            auto view = registry.view<entt::entity>();
            std::string entityText = "Entities: " + std::to_string(view.size());
            ui::UI::TextColored(entityText.c_str(), ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
            ui::UI::Separator();

            std::vector<entt::entity> cameras;
            std::vector<entt::entity> dirLights;
            std::vector<entt::entity> pointLights;
            std::vector<entt::entity> spotLights;
            std::vector<entt::entity> models;

            cameras.reserve(view.size());
            dirLights.reserve(view.size());
            pointLights.reserve(view.size());
            spotLights.reserve(view.size());
            models.reserve(view.size());

            for (auto entity : view) {
                if (registry.all_of<CameraComponent>(entity)) {
                    cameras.push_back(entity);
                    continue;
                }
                if (registry.all_of<DirectionalLightComponent>(entity)) {
                    dirLights.push_back(entity);
                    continue;
                }
                if (registry.all_of<PointLightComponent>(entity)) {
                    pointLights.push_back(entity);
                    continue;
                }
                if (registry.all_of<SpotLightComponent>(entity)) {
                    spotLights.push_back(entity);
                    continue;
                }
                if (registry.all_of<ModelComponent>(entity)) {
                    models.push_back(entity);
                    continue;
                }
            }

            // Enforce policy: keep at most one directional light entity in the scene.
            if (dirLights.size() > 1) {
                for (size_t i = 1; i < dirLights.size(); ++i) {
                    entt::entity const extra = dirLights[i];
                    if (std::find(toDelete_.begin(), toDelete_.end(), extra) == toDelete_.end()) {
                        toDelete_.push_back(extra);
                    }
                }
                dirLights.resize(1);
            }

            auto drawEntityRow = [&](entt::entity entity, const char* icon, ImVec4 color) {
                auto const id = static_cast<uint32_t>(entity);

                // ImGui::PushID takes an int; avoid implementation-defined narrowing from uint32_t.
                assert(id <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
                ImGui::PushID(static_cast<int>(id));

                std::string label = "Object " + std::to_string(id);
                if (registry.all_of<NameComponent>(entity)) {
                    label = registry.get<NameComponent>(entity).name + " " + std::to_string(id);
                }

                bool const isSelected = (frameInfo.selectedEntity == entity);

                ui::UI::TextColored(icon, color);
                ImGui::SameLine();

                ImGuiStyle const& style        = ImGui::GetStyle();
                float             actionsWidth = 0.0f;

                auto actionWidthForText = [&](const char* text) { return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f); };

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
                if (ui::UI::Selectable(label.c_str(), isSelected)) {
                    frameInfo.selectedObjectId = id;
                    frameInfo.selectedEntity   = entity;
                }
                ImGui::SameLine();

                if (registry.all_of<CameraComponent>(entity)) {
                    if (entity == frameInfo.cameraEntity) {
                        ui::UI::TextDisabled("Active");
                    } else {
                        std::string activeBtn = "Set Active##cam_" + std::to_string(id);
                        if (ui::UI::SmallButton(activeBtn.c_str())) {
                            frameInfo.cameraEntity = entity;
                        }
                    }
                    ImGui::SameLine();
                }

                if (entity == frameInfo.cameraEntity) {
                    ui::UI::TextDisabled("Delete");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Cannot delete the active camera");
                    }
                } else {
                    std::string delBtn = "Delete##del_" + std::to_string(id);
                    if (ui::UI::SmallButton(delBtn.c_str())) {
                        toDelete_.push_back(entity);
                    }
                }

                ImGui::PopID();
            };

            // --- Search filter helper ---
            auto matchesFilter = [&](entt::entity entity) -> bool {
                if (searchFilter[0] == '\0') return true;
                std::string lowFilter;
                lowFilter.reserve(strlen(searchFilter));
                for (char* p = searchFilter; *p; ++p) lowFilter += static_cast<char>(std::tolower(*p));

                std::string label;
                if (registry.all_of<NameComponent>(entity)) {
                    label = registry.get<NameComponent>(entity).name;
                    std::transform(label.begin(), label.end(), label.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (label.find(lowFilter) != std::string::npos) return true;
                }
                // Also match by entity ID
                std::string idStr = std::to_string(static_cast<uint32_t>(entity));
                if (idStr.find(lowFilter) != std::string::npos) return true;
                return false;
            };

            ImGuiTreeNodeFlags const rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth;

            {
                std::string const header = "Cameras (" + std::to_string(cameras.size()) + ")";
                ImGui::PushID("cameras_header");
                ImGuiStyle const& style = ImGui::GetStyle();
                float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
                ImGui::SetNextItemAllowOverlap();
                bool const        open  = ui::UI::TreeNode("▶", header.c_str(), rootFlags);
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
                if (ui::UI::SmallButton("+##add_camera")) {
                    auto entity = scene.createEntity();
                    registry.emplace<TransformComponent>(entity);
                    registry.emplace<CameraComponent>(entity);
                    registry.emplace<NameComponent>(entity, "Camera");
                }
                ImGui::PopID();
                if (open) {
                    for (auto entity : cameras) {
                        if (matchesFilter(entity)) {
                            drawEntityRow(entity, "[CAM]", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        }
                    }
                    ImGui::TreePop();
                }
            }

            {
                size_t const      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
                std::string const header      = "Lights (" + std::to_string(lightsTotal) + ")";
                ImGui::PushID("lights_header");
                ImGuiStyle const& style = ImGui::GetStyle();
                float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
                ImGui::SetNextItemAllowOverlap();
                bool const        open  = ui::UI::TreeNode("▶", header.c_str(), rootFlags);
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
                if (ui::UI::SmallButton("+##add_light")) {
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
                    if (ui::UI::TreeNode("  ▶", dirHeader.c_str())) {
                        for (auto entity : dirLights) {
                            if (matchesFilter(entity)) {
                                drawEntityRow(entity, "[DIR]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                            }
                        }
                        ImGui::TreePop();
                    }
                    std::string pntHeader = "Point (" + std::to_string(pointLights.size()) + ")";
                    if (ui::UI::TreeNode("  ▶", pntHeader.c_str())) {
                        for (auto entity : pointLights) {
                            if (matchesFilter(entity)) {
                                drawEntityRow(entity, "[PNT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                            }
                        }
                        ImGui::TreePop();
                    }
                    std::string sptHeader = "Spot (" + std::to_string(spotLights.size()) + ")";
                    if (ui::UI::TreeNode("  ▶", sptHeader.c_str())) {
                        for (auto entity : spotLights) {
                            if (matchesFilter(entity)) {
                                drawEntityRow(entity, "[SPT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }

            {
                std::string const header = "Models (" + std::to_string(models.size()) + ")";
                ImGui::PushID("models_header");
                ImGuiStyle const& style = ImGui::GetStyle();
                float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
                ImGui::SetNextItemAllowOverlap();
                bool const        open  = ui::UI::TreeNode("▶", header.c_str(), rootFlags);
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
                if (ui::UI::SmallButton("+##add_model")) {
                    ImGui::OpenPopup("AddModelPopup");
                }

                if (ImGui::BeginPopup("AddModelPopup")) {
                    static char filter[128] = "";
                    ui::UI::InputText("Filter", filter, sizeof(filter));

                    int colliderModeIndex = static_cast<int>(colliderImportMode_);
                    static const char* modeLabels[] = {"Auto Detect", "Force On", "Force Off"};
                    if (ui::UI::Combo("Static Mesh Collider", &colliderModeIndex, modeLabels, 3)) {
                        colliderImportMode_ = static_cast<StaticColliderImportMode>(colliderModeIndex);
                    }

                    if (colliderImportMode_ == StaticColliderImportMode::AutoDetect) {
                        std::string autoText = "Auto tokens: col_, ucx_, collision, collider, wall, floor, ground, world, level, static";
                        ui::UI::TextDisabled(autoText.c_str());
                    }
                    ui::UI::Separator();

                    std::string const indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
                    try {
                        std::ifstream f(indexPath);
                        if (!f.is_open()) {
                            std::string errText = "Failed to open model index: " + indexPath;
                            ui::UI::TextDisabled(errText.c_str());
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

                                if (filter[0] != '\0') {
                                    std::string lowName   = name;
                                    std::string lowFilter = filter;
                                    std::transform(lowName.begin(), lowName.end(), lowName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                                    std::transform(lowFilter.begin(), lowFilter.end(), lowFilter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                                    if (lowName.find(lowFilter) == std::string::npos) {
                                        continue;
                                    }
                                }

                                if (ui::UI::Selectable(name.c_str())) {
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

                                        engine::ModelInsertionOptions opts;
                                        opts.enableTextures     = true;
                                        opts.loadMaterials      = true;
                                        opts.enableMorphTargets = true;

                                        enqueueModelLoad(fullPath, name, opts, colliderImportMode_);
                                        ImGui::CloseCurrentPopup();
                                    }
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        std::string errText = "Error parsing model index: " + std::string(e.what());
                        ui::UI::TextDisabled(errText.c_str());
                    }

                    ImGui::EndPopup();
                }
                ImGui::PopID();
                if (open) {
                    for (auto entity : models) {
                        if (matchesFilter(entity)) {
                            drawEntityRow(entity, "[MDL]", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();

        ui::UI::PopThemeStyle();
    }

    void ScenePanel::processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId) {
        if (toDelete_.empty()) {
            return;
        }

        if (engineState_ == nullptr) {
            toDelete_.clear();
            return;
        }

        auto sceneState = engineState_->sceneRuntimeService().view();
        if (sceneState.scene == nullptr) {
            toDelete_.clear();
            return;
        }

        vkDeviceWaitIdle(device_.device());

        Scene& scene = *sceneState.scene;

        for (auto entity : toDelete_) {
            if (entity == selectedEntity) {
                selectedEntity   = entt::null;
                selectedObjectId = 0;
            }
            scene.destroyEntity(entity);
        }
        toDelete_.clear();
    }

}  // namespace engine
