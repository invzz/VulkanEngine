#include "Editor/SceneLoader.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "ModelLib/Resources/Model.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/fwd.hpp"

namespace engine {

    namespace {
         std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        glm::mat4 convertGLTFLightTransform(const glm::mat4& transform) {
            glm::mat4 const flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
            return flip * transform * flip;
        }

        glm::mat4 makeLightNodeTransform(const Model::Node& node) {
            if (node.hasMatrix) {
                return node.matrix;
            }

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, node.translation);
            transform *= glm::mat4_cast(node.rotation);
            transform = glm::scale(transform, node.scale);
            return transform;
        }

        bool shouldAutoCreateStaticCollider(const std::string& modelPath, const std::string& name) {
            const std::string loweredPath = toLower(modelPath);
            const std::string loweredName = toLower(name);
            const std::string combined    = loweredPath + " " + loweredName;

            static const std::vector<std::string> tokens = {
                "col_", "ucx_", "collision", "collider", "wall", "floor", "ground", "world", "level", "static"};

            for (const auto& token : tokens) {
                if (combined.find(token) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }
    }  // namespace

    void SceneLoader::loadScene(Device& device, Scene& scene, ResourceManager& resourceManager) {
        if (!scene.getRegistry().storage<entt::entity>().empty()) {
            return;
        }
    }

    void SceneLoader::createFromFile(Device& /*device*/, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath) {
        if (!scene.getRegistry().storage<entt::entity>().empty()) {
            return;
        }

        auto modelPtr = resourceManager.loadModel(modelPath, true, true, true);

        auto entity = scene.createEntity();
        scene.getRegistry().emplace<TransformComponent>(entity);
        scene.getRegistry().emplace<ModelComponent>(entity, std::move(modelPtr));
        scene.getRegistry().emplace<NameComponent>(entity, "LoadedModel");

        // Create light entities from KHR_lights_punctual data
        if (scene.getRegistry().all_of<ModelComponent>(entity)) {
            auto const& modelComp = scene.getRegistry().get<ModelComponent>(entity);
            if (modelComp.model && modelComp.model->hasLights()) {
                auto const& lights = modelComp.model->getLights();
                for (size_t lightIdx = 0; lightIdx < lights.size(); ++lightIdx) {
                    auto const& light = lights[lightIdx];

                    // Use the first node that references this light for transform
                    if (light.nodeIndices.empty()) {
                        continue;
                    }

                    auto lightEntity = scene.createEntity();
                    auto& transform  = scene.getRegistry().emplace<TransformComponent>(lightEntity);

                    // Copy transform from the referencing node
                    // (nodes are stored in same order as glTF file)
                    auto const& nodes = modelComp.model->getNodes();
                    if (light.nodeIndices[0] < static_cast<int>(nodes.size())) {
                        auto const& node = nodes[light.nodeIndices[0]];
                        glm::mat4 lightTransform = makeLightNodeTransform(node);
                        glm::mat4 engineTransform = convertGLTFLightTransform(lightTransform);
                        transform.translation = glm::vec3(engineTransform[3]);
                        transform.rotation = glm::eulerAngles(glm::quat_cast(engineTransform));
                    }
                    // Add light component based on type
                    switch (light.type) {
                        case Model::LightType::Point: {
                            auto& pl = scene.getRegistry().emplace<PointLightComponent>(lightEntity);
                            pl.color      = light.color;
                            pl.intensity  = light.intensity;
                            pl.radius     = 15.0f;  // Default point light radius
                            pl.lightType  = engine::LightMobility::Dynamic;
                            break;
                        }
                        case Model::LightType::Directional: {
                            auto& dl = scene.getRegistry().emplace<DirectionalLightComponent>(lightEntity);
                            dl.color      = light.color;
                            dl.intensity  = light.intensity;
                            dl.lightType  = engine::LightMobility::Static;
                            break;
                        }
                        case Model::LightType::Spot: {
                            auto& sl = scene.getRegistry().emplace<SpotLightComponent>(lightEntity);
                            sl.color           = light.color;
                            sl.intensity       = light.intensity;
                            sl.innerCutoffAngle = light.innerCutoffAngle;
                            sl.outerCutoffAngle = light.outerCutoffAngle;
                            sl.lightType       = engine::LightMobility::Dynamic;
                            break;
                        }
                    }

                    engine::Logger::info(engine::LogChannel::General, "[SceneLoader] Created light entity: ", light.name, " (",
                              (light.type == Model::LightType::Point ? "point" :
                                  light.type == Model::LightType::Directional ? "directional" : "spot"),
                              ") intensity=", light.intensity);
                }
            }
        }

        if (shouldAutoCreateStaticCollider(modelPath, "LoadedModel")) {
            auto& rigidBody      = scene.getRegistry().emplace<RigidBodyComponent>(entity);
            rigidBody.isStatic   = true;
            rigidBody.mode       = RigidBodyComponent::PhysicsMode::Static;
            rigidBody.useGravity = false;

            auto& collider     = scene.getRegistry().emplace<ColliderComponent>(entity);
            collider.shape     = ColliderComponent::ShapeType::Mesh;
            collider.isTrigger = false;
        }

        auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
        transform.scale       = {1.0f, 1.f, 1.0f};
        transform.translation = {0.0f, 0.0f, 0.0f};
    }

}  // namespace engine
