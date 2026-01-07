#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP

#include <entt/entt.hpp>
#include <string>

#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

  struct ModelInsertionOptions
  {
    bool                      enableTextures     = true;
    bool                      loadMaterials      = true;
    bool                      enableMorphTargets = true;
    Model::MeshletBuildConfig meshletCfg;
  };

  // Load a model (uses ResourceManager for caching) and insert it into the given
  // Scene. Returns the created entity, or throws on failure.
  entt::entity addModelToScene(ResourceManager& resourceManager, Scene& scene, const std::string& path, const std::string& name, const ModelInsertionOptions& options = {});

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SCENE_SCENEUTILS_HPP
