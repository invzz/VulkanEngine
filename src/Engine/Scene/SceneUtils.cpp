#include "Engine/Scene/SceneUtils.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include "Engine/Core/Logger.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

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
    }  // namespace

    entt::entity addModelToScene(ResourceManager& resourceManager, Scene& scene, const std::string& path, const std::string& name, const ModelInsertionOptions& options) {
        // Apply meshlet config if provided
        Model::setMeshletBuildConfig(options.meshletCfg);

        // Use ResourceManager to load model (caching + material texture loading)
        auto modelPtr = resourceManager.loadModel(path, options.enableTextures, options.loadMaterials, options.enableMorphTargets);
        if (!modelPtr) {
            throw std::runtime_error("ResourceManager failed to load model: " + path);
        }

        // Create entity and attach components
        auto entity = scene.createEntity();
        scene.getRegistry().emplace<TransformComponent>(entity);
        scene.getRegistry().emplace<ModelComponent>(entity, modelPtr);
        scene.getRegistry().emplace<NameComponent>(entity, name);

        if (shouldCreateStaticCollider(path, name, options.staticColliderMode)) {
            auto& rigidBody      = scene.getRegistry().emplace<RigidBodyComponent>(entity);
            rigidBody.isStatic   = true;
            rigidBody.mode       = RigidBodyComponent::PhysicsMode::Static;
            rigidBody.useGravity = false;

            auto& collider     = scene.getRegistry().emplace<ColliderComponent>(entity);
            collider.shape     = ColliderComponent::ShapeType::Mesh;
            collider.isTrigger = false;
        }

        auto& modelComp = scene.getRegistry().get<ModelComponent>(entity);

        if (modelComp.model->hasAnimations()) {
            scene.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
        }

        if (modelComp.model->hasMorphTargets()) {
            if (!scene.getRegistry().all_of<AnimationComponent>(entity)) {
                scene.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
            }
        }

        engine::Logger::info(engine::LogChannel::Scene, "[Model] Added to scene: ", path);
        return entity;
    }

}  // namespace engine
