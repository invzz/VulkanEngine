#include "Editor/ui/Panels/ScenePanel.hpp"

#include <imgui.h>

#include <sstream>
#include <string>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

#include "Editor/ModelLoadProcessor.hpp"
#include "Editor/ui/UI.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/entity.hpp"
namespace engine {
    ScenePanel::ScenePanel(Device& device, EngineState& state)
        : device_(device), state_(state) {}
    void ScenePanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;
        ui::UI::PushThemeStyle();
        Scene& scene    = state_.scene();
        auto&  registry = scene.getRegistry();
        auto&  rm       = state_.resourceManager();
        if (ImGui::Begin("Scene Objects", &visible_)) {
            rm.updateAsyncCallbacks();
            static char searchFilter[128] = "";
            ImGui::SetNextItemWidth(-1);
            ui::UI::InputText("##search", searchFilter, sizeof(searchFilter));
            ui::UI::Separator();
            ui::UI::DrawScenePendingLoadsSection(pendingLoads_, &rm);
            auto view = registry.view<entt::entity>();
            ui::UI::TextColored(("Entities: " + std::to_string(view.size())).c_str(),
                ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
            ui::UI::Separator();
            auto collection = ui::UI::CollectSceneEntities(scene);
            ui::UI::EnforceSingleDirectionalLight(collection.dirLights, toDelete_);
            ui::UI::DrawSceneCameraSection(collection.cameras, searchFilter,
                frameInfo, scene, registry, toDelete_);
            ui::UI::DrawSceneLightSection(collection.dirLights, collection.pointLights,
                collection.spotLights, searchFilter, frameInfo, scene, registry, toDelete_);
            auto enqueueModelLoad = [&](const std::string& fullPath, const std::string& name,
                                        const ModelInsertionOptions& opts,
                                        StaticColliderImportMode     colliderMode) {
                // Create async callback using ModelLoadProcessor
                auto callback = ModelLoadProcessor::createAsyncCallback(
                    state_.scene(), fullPath, name, colliderMode);
                // Create error callback
                auto errorCallback = [fullPath](const std::string& error) {
                    engine::Logger::error(engine::LogChannel::Scene,
                        "[Model] Async load failed for ", fullPath, ": ", error);
                };
                AsyncLoadId id = rm.enqueueModelLoad(
                    fullPath,
                    opts.enableTextures,
                    opts.loadMaterials,
                    opts.enableMorphTargets,
                    ResourcePriority::HIGH,
                    [callback, fullPath](const std::shared_ptr<Model>& modelPtr) {
                        callback(modelPtr, fullPath, entt::null);
                    },
                    errorCallback);
                pendingLoads_.push_back({id, fullPath, name, opts, colliderMode});
            };
            ui::UI::DrawSceneModelSection(collection.models, searchFilter,
                frameInfo, scene, registry, toDelete_, colliderImportMode_, enqueueModelLoad);
        }
        ImGui::End();
        ui::UI::PopThemeStyle();
    }
    void ScenePanel::processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId) {
        if (toDelete_.empty())
            return;
        vkDeviceWaitIdle(device_.device());
        Scene& scene = state_.scene();
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