#include "Engine/Scene/SceneUtils.hpp"

#include <iostream>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  entt::entity addModelToScene(ResourceManager& resourceManager, Scene& scene, const std::string& path, const std::string& name, const ModelInsertionOptions& options)
  {
    // Apply meshlet config if provided
    Model::setMeshletBuildConfig(options.meshletCfg);

    // Use ResourceManager to load model (caching + material texture loading)
    auto modelPtr = resourceManager.loadModel(path, options.enableTextures, options.loadMaterials, options.enableMorphTargets);
    if (!modelPtr)
    {
      throw std::runtime_error("ResourceManager failed to load model: " + path);
    }

    // Create entity and attach components
    auto entity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(entity);
    scene.getRegistry().emplace<ModelComponent>(entity, modelPtr);
    scene.getRegistry().emplace<NameComponent>(entity, name);

    auto& modelComp = scene.getRegistry().get<ModelComponent>(entity);

    if (modelComp.model->hasAnimations())
    {
      scene.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
    }

    if (modelComp.model->hasMorphTargets())
    {
      if (!scene.getRegistry().all_of<AnimationComponent>(entity))
      {
        scene.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
      }
    }

    std::cout << "[Model] Added to scene: " << path << "\n";
    return entity;
  }

} // namespace engine
