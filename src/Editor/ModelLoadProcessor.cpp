#include "Editor/ModelLoadProcessor.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/euler_angles.hpp"
namespace engine {
    namespace {
        glm::mat4 convertGLTFLightTransform(const glm::mat4& transform) {
            glm::mat4 const flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
            return flip * transform * flip;
        }
        glm::mat4 makeLightNodeTransform(const Model::Node& node) {
            if (node.hasMatrix) {
                return node.matrix;
            }
            glm::mat4 transform = glm::mat4(1.0f);
            transform           = glm::translate(transform, node.translation);
            transform *= glm::mat4_cast(node.rotation);
            transform = glm::scale(transform, node.scale);
            return transform;
        }
        bool shouldAutoCreateStaticCollider(const std::string& modelPath, const std::string& name) {
            std::string loweredPath = modelPath;
            std::string loweredName = name;
            std::transform(loweredPath.begin(), loweredPath.end(), loweredPath.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(loweredName.begin(), loweredName.end(), loweredName.begin(), [](unsigned char c) { return std::tolower(c); });
            const std::string                     combined = loweredPath + " " + loweredName;
            static const std::vector<std::string> tokens   = {
                "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};
            for (const auto& token : tokens) {
                if (combined.find(token) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }
    }  // anonymous namespace
    void ModelLoadProcessor::processLoadedModel(
        Scene&                                          scene,
        const std::shared_ptr<Model>&                   modelPtr,
        const std::string&                              modelPath,
        const std::string&                              modelName,
        ModelInsertionOptions::StaticColliderImportMode colliderMode) {
        if (!modelPtr) {
            Logger::error(LogChannel::Scene, "[ModelLoadProcessor] Cannot process null model: ", modelPath);
            return;
        }
        auto& registry = scene.getRegistry();
        auto  entity   = scene.createEntity();
        registry.emplace<TransformComponent>(entity);
        registry.emplace<ModelComponent>(entity, modelPtr);
        registry.emplace<NameComponent>(entity, modelName);
        // Handle static collider
        if (shouldCreateStaticCollider(modelPath, modelName, colliderMode)) {
            auto& rb      = registry.emplace<RigidBodyComponent>(entity);
            rb.isStatic   = true;
            rb.mode       = RigidBodyComponent::PhysicsMode::Static;
            rb.useGravity = false;
            auto& col     = registry.emplace<ColliderComponent>(entity);
            col.shape     = ColliderComponent::ShapeType::Mesh;
            col.isTrigger = false;
        }
        // Handle animations and morph targets
        if (modelPtr->hasAnimations() || modelPtr->hasMorphTargets()) {
            registry.emplace<AnimationComponent>(entity, modelPtr);
        }
        // Handle embedded lights
        createLightEntities(scene, *modelPtr);
        Logger::info(LogChannel::Scene, "[ModelLoadProcessor] Added model to scene: ", modelPath);
    }
    ModelLoadProcessor::LoadCallback ModelLoadProcessor::createAsyncCallback(
        Scene&                                          scene,
        const std::string&                              modelPath,
        const std::string&                              modelName,
        ModelInsertionOptions::StaticColliderImportMode colliderMode) {
        return [&scene, modelPath, modelName, colliderMode](
                   const std::shared_ptr<Model>& modelPtr,
                   const std::string& /*path*/,
                   entt::entity /*entity*/
               ) {
            if (!modelPtr) {
                Logger::error(LogChannel::Scene, "[ModelLoadProcessor] Async load returned null: ", modelPath);
                return;
            }
            processLoadedModel(scene, modelPtr, modelPath, modelName, colliderMode);
        };
    }
    void ModelLoadProcessor::createLightEntities(
        Scene&       scene,
        const Model& model) {
        if (!model.hasLights()) {
            return;
        }
        auto const& lights = model.getLights();
        auto const& nodes  = model.getNodes();
        for (auto const& light : lights) {
            if (light.nodeIndices.empty()) {
                continue;
            }
            auto  lightEntity = scene.createEntity();
            auto& transform   = scene.getRegistry().emplace<TransformComponent>(lightEntity);
            if (!light.nodeIndices.empty() && light.nodeIndices[0] < static_cast<int>(nodes.size())) {
                auto const& node            = nodes[light.nodeIndices[0]];
                glm::mat4   lightTransform  = makeLightNodeTransform(node);
                glm::mat4   engineTransform = convertGLTFLightTransform(lightTransform);
                transform.translation       = glm::vec3(engineTransform[3]);
                transform.rotation          = glm::eulerAngles(glm::quat_cast(engineTransform));
            }
            scene.getRegistry().emplace<NameComponent>(lightEntity, light.name);
            switch (light.type) {
                case Model::LightType::Point: {
                    auto& pl     = scene.getRegistry().emplace<PointLightComponent>(lightEntity);
                    pl.color     = light.color;
                    pl.intensity = light.intensity;
                    pl.radius    = 15.0f;
                    pl.lightType = LightMobility::Dynamic;
                    break;
                }
                case Model::LightType::Directional: {
                    auto& dl     = scene.getRegistry().emplace<DirectionalLightComponent>(lightEntity);
                    dl.color     = light.color;
                    dl.intensity = light.intensity;
                    dl.lightType = LightMobility::Static;
                    break;
                }
                case Model::LightType::Spot: {
                    auto& sl            = scene.getRegistry().emplace<SpotLightComponent>(lightEntity);
                    sl.color            = light.color;
                    sl.intensity        = light.intensity;
                    sl.innerCutoffAngle = light.innerCutoffAngle;
                    sl.outerCutoffAngle = light.outerCutoffAngle;
                    sl.lightType        = LightMobility::Dynamic;
                    break;
                }
            }
            std::string lightTypeStr;
            if (light.type == Model::LightType::Point) {
                lightTypeStr = "point";
            } else if (light.type == Model::LightType::Directional) {
                lightTypeStr = "directional";
            } else {
                lightTypeStr = "spot";
            }
            Logger::info(LogChannel::Scene, "[ModelLoadProcessor] Created light entity: ", light.name, " (",
                lightTypeStr, ") intensity=", light.intensity);
        }
    }
    bool ModelLoadProcessor::shouldCreateStaticCollider(
        const std::string&                              modelPath,
        const std::string&                              name,
        ModelInsertionOptions::StaticColliderImportMode mode) {
        switch (mode) {
            case ModelInsertionOptions::StaticColliderImportMode::ForceOn:
                return true;
            case ModelInsertionOptions::StaticColliderImportMode::ForceOff:
                return false;
            case ModelInsertionOptions::StaticColliderImportMode::AutoDetect:
            default:
                return shouldAutoCreateStaticCollider(modelPath, name);
        }
    }
}  // namespace engine
