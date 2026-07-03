#include "Editor/ui/Panels/ScenePanel.hpp"

#include <imgui.h>

#include <sstream>
#include <string>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/entity.hpp"

namespace {
    glm::mat4 convertGLTFLightTransform(const glm::mat4& transform) {
        glm::mat4 const flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
        return flip * transform * flip;
    }

    glm::mat4 makeLightNodeTransform(const engine::Model::Node& node) {
        if (node.hasMatrix) {
            return node.matrix;
        }

        glm::mat4 transform = glm::mat4(1.0f);
        transform           = glm::translate(transform, node.translation);
        transform *= glm::mat4_cast(node.rotation);
        transform = glm::scale(transform, node.scale);
        return transform;
    }
}  // namespace

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
                AsyncLoadId id = rm.enqueueModelLoad(fullPath, opts.enableTextures, opts.loadMaterials, opts.enableMorphTargets, ResourcePriority::HIGH, [this, fullPath, name, colliderMode](const std::shared_ptr<Model>& modelPtr) {
                        if (!modelPtr) {
                            engine::Logger::error(engine::LogChannel::Scene, "[Model] Async load returned null: ", fullPath);
                            return;
                        }
                        Scene& s    = state_.scene();
                        auto&  reg  = s.getRegistry();
                        auto entity = s.createEntity();
                        reg.emplace<TransformComponent>(entity);
                        reg.emplace<ModelComponent>(entity, modelPtr);
                        reg.emplace<NameComponent>(entity, name);

                        if (ui::UI::ShouldCreateStaticCollider(fullPath, name, colliderMode)) {
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

                        
                        if (mc.model->hasLights()) {
                            auto const& lights = mc.model->getLights();
                            for (auto const& light : lights) {
                                if (light.nodeIndices.empty())
                                    continue;

                                auto lightEntity = s.createEntity();
                                auto& transform  = s.getRegistry().emplace<TransformComponent>(lightEntity);

                                auto const& nodes = mc.model->getNodes();
                                if (light.nodeIndices[0] < static_cast<int>(nodes.size())) {
                                    auto const& node = nodes[light.nodeIndices[0]];
                                    glm::mat4 lightTransform = makeLightNodeTransform(node);
                                    glm::mat4 engineTransform = convertGLTFLightTransform(lightTransform);
                                    transform.translation = glm::vec3(engineTransform[3]);
                                    transform.rotation = glm::eulerAngles(glm::quat_cast(engineTransform));
                                }

                                s.getRegistry().emplace<NameComponent>(lightEntity, light.name);

                                switch (light.type) {
                                    case Model::LightType::Point: {
                                        auto& pl = s.getRegistry().emplace<PointLightComponent>(lightEntity);
                                        pl.color      = light.color;
                                        pl.intensity  = light.intensity;
                                        pl.radius     = 15.0f;
                                        pl.lightType  = engine::LightMobility::Dynamic;
                                        break;
                                    }
                                    case Model::LightType::Directional: {
                                        auto& dl = s.getRegistry().emplace<DirectionalLightComponent>(lightEntity);
                                        dl.color      = light.color;
                                        dl.intensity  = light.intensity;
                                        dl.lightType  = engine::LightMobility::Static;
                                        break;
                                    }
                                    case Model::LightType::Spot: {
                                        auto& sl = s.getRegistry().emplace<SpotLightComponent>(lightEntity);
                                        sl.color           = light.color;
                                        sl.intensity       = light.intensity;
                                        sl.innerCutoffAngle = light.innerCutoffAngle;
                                        sl.outerCutoffAngle = light.outerCutoffAngle;
                                        sl.lightType       = engine::LightMobility::Dynamic;
                                        break;
                                    }
                                }

                                engine::Logger::info(engine::LogChannel::Scene, "[Model] Created light entity: ", light.name, " (",
                                          (light.type == Model::LightType::Point ? "point" :
                                              light.type == Model::LightType::Directional ? "directional" : "spot"),
                                          ") intensity=", light.intensity);
                            }
                        }

                        engine::Logger::info(engine::LogChannel::Scene, "[Model] Added to scene (async): ", fullPath); }, [fullPath](const std::string& error) { engine::Logger::error(engine::LogChannel::Scene, "[Model] Async load failed for ", fullPath, ": ", error); });

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