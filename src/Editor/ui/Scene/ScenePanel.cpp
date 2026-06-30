#include "Editor/ui/Scene/ScenePanel.hpp"

#include <imgui.h>

#include <iostream>
#include <string>
#include <vector>

#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/Scene/SceneComponents.hpp"
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

            // Search filter
            static char searchFilter[128] = "";
            ImGui::SetNextItemWidth(-1);
            ui::UI::InputText("##search", searchFilter, sizeof(searchFilter));
            ui::UI::Separator();

            // Pending loads
            ui::SceneComponents::drawPendingLoadsSection(pendingLoads_, &rm);

            // Entity count
            auto view = registry.view<entt::entity>();
            ui::UI::TextColored(("Entities: " + std::to_string(view.size())).c_str(),
                ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
            ui::UI::Separator();

            auto collection = ui::SceneComponents::collectEntities(scene);
            ui::SceneComponents::enforceSingleDirectionalLight(collection.dirLights, toDelete_);

            ui::SceneComponents::drawCameraSection(collection.cameras, searchFilter,
                frameInfo, scene, registry, toDelete_);

            ui::SceneComponents::drawLightSection(collection.dirLights, collection.pointLights,
                collection.spotLights, searchFilter, frameInfo, scene, registry, toDelete_);

            auto enqueueModelLoad = [&](const std::string& fullPath, const std::string& name,
                                        const ModelInsertionOptions& opts,
                                        StaticColliderImportMode     colliderMode) {
                AsyncLoadId id = rm.enqueueModelLoad(fullPath, opts.enableTextures, opts.loadMaterials, opts.enableMorphTargets, ResourcePriority::HIGH, [this, fullPath, name, colliderMode](const std::shared_ptr<Model>& modelPtr) {
                        if (!modelPtr) {
                            std::cerr << "[Model] Async load returned null: " << fullPath << "\n";
                            return;
                        }
                        Scene& s    = state_.scene();
                        auto&  reg  = s.getRegistry();
                        auto entity = s.createEntity();
                        reg.emplace<TransformComponent>(entity);
                        reg.emplace<ModelComponent>(entity, modelPtr);
                        reg.emplace<NameComponent>(entity, name);

                        if (ui::SceneComponents::shouldCreateStaticCollider(fullPath, name, colliderMode)) {
                            auto& rb = reg.emplace<RigidBodyComponent>(entity);
                            rb.isStatic = true;
                            rb.mode = RigidBodyComponent::PhysicsMode::Static;
                            rb.useGravity = false;
                            auto& col = reg.emplace<ColliderComponent>(entity);
                            col.shape = ColliderComponent::ShapeType::Mesh;
                            col.isTrigger = false;
                        }

                        auto& mc = reg.get<ModelComponent>(entity);
                        if (mc.model->hasAnimations())
                            reg.emplace<AnimationComponent>(entity, mc.model);
                        if (mc.model->hasMorphTargets() && !reg.all_of<AnimationComponent>(entity))
                            reg.emplace<AnimationComponent>(entity, mc.model);

                        std::cout << "[Model] Added to scene (async): " << fullPath << "\n"; }, [fullPath](const std::string& error) { std::cerr << "[Model] Async load failed for " << fullPath << ": " << error << "\n"; });

                pendingLoads_.push_back({id, fullPath, name, opts, colliderMode});
            };

            ui::SceneComponents::drawModelSection(collection.models, searchFilter,
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