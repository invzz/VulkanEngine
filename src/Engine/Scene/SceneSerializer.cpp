#include "Engine/Scene/SceneSerializer.hpp"

#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/LODComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

// Helper for glm serialization
namespace glm {
  void to_json(nlohmann::json& j, const vec3& v)
  {
    j = nlohmann::json{v.x, v.y, v.z};
  }

  void from_json(const nlohmann::json& j, vec3& v)
  {
    v.x = j[0];
    v.y = j[1];
    v.z = j[2];
  }
} // namespace glm

namespace engine {

  SceneSerializer::SceneSerializer(Scene& scene, ResourceManager& resourceManager) : scene(scene), resourceManager(resourceManager) {}

  void SceneSerializer::serialize(const std::string& filepath)
  {
    nlohmann::json sceneJson;
    sceneJson["objects"] = nlohmann::json::array();

    auto view = scene.getRegistry().view<entt::entity>();
    for (auto entity : view)
    {
      nlohmann::json objJson;
      objJson["id"] = (uint32_t)entity;
      if (scene.getRegistry().all_of<NameComponent>(entity))
      {
        objJson["name"] = scene.getRegistry().get<NameComponent>(entity).name;
      }
      else
      {
        objJson["name"] = "GameObject";
      }

      // Transform
      if (scene.getRegistry().all_of<TransformComponent>(entity))
      {
        auto& t              = scene.getRegistry().get<TransformComponent>(entity);
        objJson["transform"] = {{"translation", t.translation}, {"rotation", t.rotation}, {"scale", t.scale}};
      }

      // PBR Material & Model
      if (scene.getRegistry().all_of<ModelComponent>(entity))
      {
        auto& modelComp = scene.getRegistry().get<ModelComponent>(entity);
        if (modelComp.model)
        {
          objJson["modelPath"] = modelComp.model->getFilePath();

          if (scene.getRegistry().all_of<PBRMaterial>(entity))
          {
            auto&          mat = scene.getRegistry().get<PBRMaterial>(entity);
            nlohmann::json matJson;
            matJson["albedo"]    = mat.albedo;
            matJson["metallic"]  = mat.metallic;
            matJson["roughness"] = mat.roughness;
            matJson["ao"]        = mat.ao;
            objJson["material"]  = matJson;
          }
        }
      }

      // Lights
      if (scene.getRegistry().all_of<PointLightComponent>(entity))
      {
        auto& pl              = scene.getRegistry().get<PointLightComponent>(entity);
        objJson["pointLight"] = {{"intensity", pl.intensity}, {"color", pl.color}, {"radius", pl.radius}};
      }

      if (scene.getRegistry().all_of<DirectionalLightComponent>(entity))
      {
        auto& dl                    = scene.getRegistry().get<DirectionalLightComponent>(entity);
        objJson["directionalLight"] = {{"intensity", dl.intensity}, {"color", dl.color}};
      }

      if (scene.getRegistry().all_of<SpotLightComponent>(entity))
      {
        auto& sl             = scene.getRegistry().get<SpotLightComponent>(entity);
        objJson["spotLight"] = {{"intensity", sl.intensity}, {"color", sl.color}, {"innerAngle", sl.innerCutoffAngle}, {"outerAngle", sl.outerCutoffAngle}};
      }

      // LOD Component
      if (scene.getRegistry().all_of<LODComponent>(entity))
      {
        auto&          lod     = scene.getRegistry().get<LODComponent>(entity);
        nlohmann::json lodJson = nlohmann::json::array();
        for (const auto& level : lod.levels)
        {
          if (level.model)
          {
            lodJson.push_back({{"distance", level.distance}, {"modelPath", level.model->getFilePath()}});
          }
        }
        objJson["lodComponent"] = lodJson;
      }

      sceneJson["objects"].push_back(objJson);
    }

    std::ofstream out(filepath);
    out << sceneJson.dump(4);
    out.close();
  }

  bool SceneSerializer::deserialize(const std::string& filepath)
  {
    std::ifstream in(filepath);
    if (!in.is_open())
    {
      std::cerr << "Failed to open scene file: " << filepath << std::endl;
      return false;
    }

    nlohmann::json sceneJson;
    try
    {
      in >> sceneJson;
    }
    catch (const std::exception& e)
    {
      std::cerr << "Failed to parse scene file: " << e.what() << std::endl;
      return false;
    }

    scene.getRegistry().clear(); // Clear existing objects

    if (sceneJson.contains("objects"))
    {
      for (const auto& objJson : sceneJson["objects"])
      {
        std::string name = objJson.value("name", "GameObject");

        auto entity = scene.createEntity();
        scene.getRegistry().emplace<TransformComponent>(entity);
        scene.getRegistry().emplace<NameComponent>(entity, name);

        // Transform
        if (objJson.contains("transform"))
        {
          auto& t               = objJson["transform"];
          auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
          transform.translation = t.value("translation", glm::vec3(0.0f));
          transform.rotation    = t.value("rotation", glm::vec3(0.0f));
          transform.scale       = t.value("scale", glm::vec3(1.0f));
        }

        // Model & Material
        if (objJson.contains("modelPath"))
        {
          std::string modelPath = objJson["modelPath"];
          auto        model     = resourceManager.loadModel(modelPath, true, true, true);
          scene.getRegistry().emplace<ModelComponent>(entity, model);

          if (objJson.contains("material"))
          {
            auto& matJson         = objJson["material"];
            auto& pbrMaterial     = scene.getRegistry().emplace<PBRMaterial>(entity);
            pbrMaterial.albedo    = matJson.value("albedo", glm::vec3(1.0f));
            pbrMaterial.metallic  = matJson.value("metallic", 0.0f);
            pbrMaterial.roughness = matJson.value("roughness", 0.5f);
            pbrMaterial.ao        = matJson.value("ao", 1.0f);
          }
        }

        // Lights
        if (objJson.contains("pointLight"))
        {
          auto& pl             = objJson["pointLight"];
          auto& pointLight     = scene.getRegistry().emplace<PointLightComponent>(entity);
          pointLight.intensity = pl.value("intensity", 1.0f);
          pointLight.color     = pl.value("color", glm::vec3(1.0f));
          pointLight.radius    = pl.value("radius", 0.1f);
        }

        if (objJson.contains("directionalLight"))
        {
          auto& dl           = objJson["directionalLight"];
          auto& dirLight     = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
          dirLight.intensity = dl.value("intensity", 1.0f);
          dirLight.color     = dl.value("color", glm::vec3(1.0f));
        }

        if (objJson.contains("spotLight"))
        {
          auto& sl                   = objJson["spotLight"];
          auto& spotLight            = scene.getRegistry().emplace<SpotLightComponent>(entity);
          spotLight.intensity        = sl.value("intensity", 1.0f);
          spotLight.color            = sl.value("color", glm::vec3(1.0f));
          spotLight.innerCutoffAngle = sl.value("innerAngle", 12.5f);
          spotLight.outerCutoffAngle = sl.value("outerAngle", 17.5f);
        }

        // LOD Component
        if (objJson.contains("lodComponent"))
        {
          auto& lodComponent = scene.getRegistry().emplace<LODComponent>(entity);
          for (const auto& levelJson : objJson["lodComponent"])
          {
            float       distance  = levelJson.value("distance", 0.0f);
            std::string modelPath = levelJson.value("modelPath", "");
            if (!modelPath.empty())
            {
              auto model = resourceManager.loadModel(modelPath, true, true, true);
              lodComponent.levels.push_back({model, distance});
            }
          }
        }
      }
      return true;
    }
    return false;
  }

} // namespace engine
