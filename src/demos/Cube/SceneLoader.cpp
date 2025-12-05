#include "SceneLoader.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  void SceneLoader::loadScene(Device& device, Scene& scene, ResourceManager& resourceManager)
  {
    if (scene.getRegistry().storage<entt::entity>().size() > 0)
    {
      return;
    }
    createDemoObject(device, scene, resourceManager);
  }

  void SceneLoader::createFromFile(Device& device, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath)
  {
    if (scene.getRegistry().storage<entt::entity>().size() > 0)
    {
      return;
    }

    auto modelPtr = resourceManager.loadModel(modelPath, false, true, true);

    auto entity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(entity);
    scene.getRegistry().emplace<ModelComponent>(entity, std::move(modelPtr));
    scene.getRegistry().emplace<PBRMaterial>(entity);
    scene.getRegistry().emplace<NameComponent>(entity, "LoadedModel");

    auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
    transform.scale       = {1.0f, 1.f, 1.0f};
    transform.translation = {0.0f, 0.0f, 0.0f};
  }

  void SceneLoader::createApple(Device& device, Scene& scene, ResourceManager& resourceManager)
  {
    auto modelPtr = resourceManager.loadModel(MODEL_PATH "/3DApple002_SQ-4K-JPG.obj", false, true, true);

    auto entity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(entity);
    scene.getRegistry().emplace<ModelComponent>(entity, std::move(modelPtr));
    scene.getRegistry().emplace<PBRMaterial>(entity);
    scene.getRegistry().emplace<NameComponent>(entity, "Apple");

    auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
    transform.scale       = {5.0f, 5.f, 5.0f};
    transform.translation = {0.0f, 0.0f, 0.0f};

    auto&       modelComp = scene.getRegistry().get<ModelComponent>(entity);
    const auto& materials = modelComp.model->getMaterials();
    if (!materials.empty())
    {
      const auto& mat      = materials[0];
      std::string basePath = std::string(TEXTURE_PATH) + "/3DApple002_SQ-4K-JPG/";

      auto& pbrMat = scene.getRegistry().get<PBRMaterial>(entity);

      if (!mat.diffuseTexPath.empty())
      {
        pbrMat.albedoMap = resourceManager.loadTexture(basePath + mat.diffuseTexPath, true);
      }

      if (!mat.normalTexPath.empty())
      {
        pbrMat.normalMap = resourceManager.loadTexture(basePath + mat.normalTexPath, false);
      }

      if (!mat.roughnessTexPath.empty())
      {
        pbrMat.roughnessMap = resourceManager.loadTexture(basePath + mat.roughnessTexPath, false);
      }

      if (!mat.aoTexPath.empty())
      {
        pbrMat.aoMap = resourceManager.loadTexture(basePath + mat.aoTexPath, false);
      }
      pbrMat.uvScale = 1.0f;
    }
  }

  void SceneLoader::createSpaceShip(Device& device, Scene& scene, ResourceManager& resourceManager)
  {
    auto spaceShipModel = resourceManager.loadModel(MODEL_PATH "/SpaceShipModeling2.obj", false, true, false);

    auto entity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(entity);
    scene.getRegistry().emplace<ModelComponent>(entity, spaceShipModel);
    scene.getRegistry().emplace<PBRMaterial>(entity);
    scene.getRegistry().emplace<NameComponent>(entity, "SpaceShip");

    auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
    transform.scale       = {0.2f, 0.2f, 0.2f};
    transform.translation = {0.0f, 0.0f, 0.0f};
  }

  void SceneLoader::createLights(Scene& scene, float radius)
  {
    std::vector<glm::vec3> lightColors = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}};

    for (size_t i = 0; i < lightColors.size(); i++)
    {
      auto entity = scene.createEntity();
      scene.getRegistry().emplace<TransformComponent>(entity);
      scene.getRegistry().emplace<PointLightComponent>(entity, 1.0f, lightColors[i], 0.05f);
      scene.getRegistry().emplace<NameComponent>(entity, "PointLight" + std::to_string(i));

      auto rotateLight =
              glm::rotate(glm::mat4(1.0f), (glm::two_pi<float>() * static_cast<float>(i)) / static_cast<float>(lightColors.size()), glm::vec3(0.f, -1.f, 0.f));
      scene.getRegistry().get<TransformComponent>(entity).translation = glm::vec3(rotateLight * glm::vec4{-radius, -2.f, -radius, 1.f});
    }

    auto dirEntity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(dirEntity);
    scene.getRegistry().emplace<DirectionalLightComponent>(dirEntity, 0.5f, glm::vec3{1.0f, 0.95f, 0.9f});
    scene.getRegistry().emplace<NameComponent>(dirEntity, "DirectionalLight");

    auto& transform       = scene.getRegistry().get<TransformComponent>(dirEntity);
    transform.translation = {0.0f, -5.0f, 0.0f};
    transform.rotation    = {0.5f, 0.0f, 0.0f}; // Pointing down-ish
  }

  void SceneLoader::createFloor(Device& device, Scene& scene, ResourceManager& resourceManager)
  {
    auto floorModel = resourceManager.loadModel(MODEL_PATH "/quad.obj", false, true, true);

    auto entity = scene.createEntity();
    scene.getRegistry().emplace<TransformComponent>(entity);
    scene.getRegistry().emplace<ModelComponent>(entity, floorModel);
    scene.getRegistry().emplace<PBRMaterial>(entity);
    scene.getRegistry().emplace<NameComponent>(entity, "Floor");

    auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
    transform.scale       = {10.0f, 0.1f, 10.0f};
    transform.translation = {0.0f, 2.0f, 0.0f};

    auto& material     = scene.getRegistry().get<PBRMaterial>(entity);
    material.albedo    = {0.5f, 0.5f, 0.5f, 1.0f};
    material.roughness = 0.8f;
  }

  void SceneLoader::createDemoObject(Device& device, Scene& scene, ResourceManager& resourceManager)
  {
    // Just create the apple for now as demo object
    createApple(device, scene, resourceManager);
  }

} // namespace engine
