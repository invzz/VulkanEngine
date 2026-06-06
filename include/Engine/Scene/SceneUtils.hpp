#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP

#include <entt/entt.hpp>
#include <string>

#include "Engine/Scene/Scene.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    struct ModelInsertionOptions {
        enum class StaticColliderImportMode : uint8_t {
            AutoDetect = 0,
            ForceOn    = 1,
            ForceOff   = 2,
        };

        bool                      enableTextures     = true;
        bool                      loadMaterials      = true;
        bool                      enableMorphTargets = true;
        StaticColliderImportMode  staticColliderMode = StaticColliderImportMode::AutoDetect;
        Model::MeshletBuildConfig meshletCfg;
    };

    // Load a model (uses ResourceManager for caching) and insert it into the given
    // Scene. Returns the created entity, or throws on failure.
    entt::entity addModelToScene(ResourceManager& resourceManager, Scene& scene, const std::string& path, const std::string& name, const ModelInsertionOptions& options = {});

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP
