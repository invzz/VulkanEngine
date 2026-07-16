// STL
#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Third party
#include <imgui.h>

#include <nlohmann/json.hpp>

// Engine
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/ChildComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/NodeIndexComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/SubMeshComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

// Editor / UI
#include "Editor/ui/ModelBrowser.hpp"
#include "Editor/ui/ModelInfoPane.hpp"
#include "Editor/ui/SceneTree.hpp"
#include "Editor/ui/SceneTreeHelpers.hpp"
#include "Editor/ui/StaticColliderRules.hpp"
#include "Editor/ui/Theme.hpp"
#include "Editor/ui/UI.hpp"
#include "Editor/ui/UIHelpers.hpp"
#include "IconsFontAwesome6.h"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine::ui {

    namespace {

        // ---------------------------------------------------------------
        // Root detection (used by CollectSceneEntities).
        // ---------------------------------------------------------------

        bool isRootEntity(const entt::registry& registry, entt::entity entity) {
            return !registry.all_of<ChildComponent>(entity);
        }

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

        SectionHeaderResult drawSectionHeaderWithAddButton(const char* pushIdLabel,
            const char*                                                icon,
            const std::string&                                         header,
            const char*                                                addButtonId,
            ImGuiTreeNodeFlags                                         flags = 0) {
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

    void UI::EnforceSingleDirectionalLight(std::vector<entt::entity>& dirLights,
        std::vector<entt::entity>&                                    toDelete) {
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

    void UI::DrawSceneCameraSection(const std::vector<entt::entity>& cameras,
        const char*                                                  filter,
        FrameInfo&                                                   frameInfo,
        engine::Scene&                                               scene,
        entt::registry&                                              registry,
        std::vector<entt::entity>&                                   toDelete) {
        const std::string header = "Cameras (" + std::to_string(cameras.size()) + ")";
        const auto        result = drawSectionHeaderWithAddButton(
            "cameras_header", ICON_FA_CAMERA, header, "+##add_camera", ImGuiTreeNodeFlags_DefaultOpen);

        if (result.addClicked) {
            createNamedEntity<CameraComponent>(scene, registry, "Camera");
        }

        if (result.open) {
            for (auto entity : cameras) {
                if (!MatchesFilter(entityDisplayName(registry, entity), filter)) {
                    continue;
                }
                drawEntityRow(entity, "[CAM]", Theme::Camera, frameInfo, registry, toDelete);
            }
            ImGui::TreePop();
        }
    }

    void UI::DrawSceneLightSection(const std::vector<entt::entity>& dirLights,
        const std::vector<entt::entity>&                            pointLights,
        const std::vector<entt::entity>&                            spotLights,
        const char*                                                 filter,
        FrameInfo&                                                  frameInfo,
        engine::Scene&                                              scene,
        entt::registry&                                             registry,
        std::vector<entt::entity>&                                  toDelete) {
        const size_t      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
        const std::string header      = "Lights (" + std::to_string(lightsTotal) + ")";
        const auto        result      = drawSectionHeaderWithAddButton(
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
                        if (!MatchesFilter(entityDisplayName(registry, entity), filter)) {
                            continue;
                        }
                        drawEntityRow(entity, icon, Theme::Light, frameInfo, registry, toDelete);
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

    void UI::DrawSceneModelSection(engine::Device&       device,
        const std::vector<entt::entity>&                 models,
        const char*                                      filter,
        FrameInfo&                                       frameInfo,
        engine::Scene&                                   scene,
        entt::registry&                                  registry,
        std::vector<entt::entity>&                       toDelete,
        ModelInsertionOptions::StaticColliderImportMode& colliderMode,
        std::function<void(const std::string&, const std::string&, const ModelInsertionOptions&,
            ModelInsertionOptions::StaticColliderImportMode)>
            enqueueModelLoad) {
        (void) scene;

        // The browser keeps its popup state (filter, active model) alive across
        // frames; construct it once and reuse.
        static ModelBrowser modelBrowser(device);

        const std::string header = "Models (" + std::to_string(models.size()) + ")";
        const auto        result = drawSectionHeaderWithAddButton(
            "models_header", ICON_FA_CUBE, header, "+##add_model", ImGuiTreeNodeFlags_DefaultOpen);

        if (result.addClicked) {
            modelBrowser.Open();
        }

        modelBrowser.Draw(colliderMode, enqueueModelLoad);

        if (result.open) {
            SceneTree tree(registry);
            for (auto entity : models) {
                if (!MatchesFilter(entityDisplayName(registry, entity), filter)) {
                    continue;
                }
                tree.DrawEntity(entity, ICON_FA_CUBE, Theme::Model, frameInfo, toDelete);
            }
            ImGui::TreePop();
        }
    }

    void UI::DrawScenePendingLoadsSection(std::vector<ScenePendingModelLoad>& pendingLoads,
        ResourceManager*                                                      resourceManager) {
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

    bool UI::ShouldCreateStaticCollider(const std::string& path,
        const std::string&                                 name,
        ModelInsertionOptions::StaticColliderImportMode    mode) {
        return engine::StaticColliderRules::ShouldCreate(path, name, mode);
    }

}  // namespace engine::ui
