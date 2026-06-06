#include "Editor/ui/ScenePanel.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"
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
#include "Editor/ui/Scene.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

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

        Scene& scene     = *sceneState.scene;
        auto&  registry  = scene.getRegistry();
        auto   resources = engineState_->resourceService().view();

        if (ImGui::Begin("Scene Objects", &visible_)) {
            if (resources.resourceManager != nullptr) {
                resources.resourceManager->updateAsyncCallbacks();
            }

            // --- Search filter ---
            static char searchFilter[128] = "";
            ImGui::SetNextItemWidth(-1);
            ui::UI::InputText("##search", searchFilter, sizeof(searchFilter));
            ui::UI::Separator();

            // --- Pending loads ---
            ui::SceneUI::drawPendingLoadsSection(pendingLoads_, resources.resourceManager);

            // --- Entity count ---
            auto        view       = registry.view<entt::entity>();
            std::string entityText = "Entities: " + std::to_string(view.size());
            ui::UI::TextColored(entityText.c_str(), ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
            ui::UI::Separator();

            // --- Collect entities ---
            auto collection = ui::SceneUI::collectEntities(scene);

            // --- Enforce single directional light policy ---
            ui::SceneUI::enforceSingleDirectionalLight(collection.dirLights, toDelete_);

            // --- Draw sections ---
            ui::SceneUI::drawCameraSection(collection.cameras,
                searchFilter,
                frameInfo,
                scene,
                registry,
                toDelete_);

            ui::SceneUI::drawLightSection(collection.dirLights,
                collection.pointLights,
                collection.spotLights,
                searchFilter,
                frameInfo,
                scene,
                registry,
                toDelete_);

            // --- Models section ---
            auto enqueueModelLoad = [&](const std::string&           fullPath,
                                        const std::string&           name,
                                        const ModelInsertionOptions& opts,
                                        StaticColliderImportMode     colliderMode) {
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

                        Scene& scene    = *sceneState.scene;
                        auto&  registry = scene.getRegistry();

                        auto entity = scene.createEntity();
                        registry.emplace<TransformComponent>(entity);
                        registry.emplace<ModelComponent>(entity, modelPtr);
                        registry.emplace<NameComponent>(entity, name);

                        if (ui::SceneUI::shouldCreateStaticCollider(fullPath, name, colliderMode)) {
                            auto& rigidBody      = registry.emplace<RigidBodyComponent>(entity);
                            rigidBody.isStatic   = true;
                            rigidBody.mode       = RigidBodyComponent::PhysicsMode::Static;
                            rigidBody.useGravity = false;

                            auto& collider     = registry.emplace<ColliderComponent>(entity);
                            collider.shape     = ColliderComponent::ShapeType::Mesh;
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

                ui::SceneUI::PendingModelLoad pending;
                pending.id           = id;
                pending.path         = fullPath;
                pending.name         = name;
                pending.options      = opts;
                pending.colliderMode = colliderMode;
                pendingLoads_.emplace_back(std::move(pending));
            };

            ui::SceneUI::drawModelSection(collection.models,
                searchFilter,
                frameInfo,
                scene,
                registry,
                toDelete_,
                colliderImportMode_,
                enqueueModelLoad);
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
