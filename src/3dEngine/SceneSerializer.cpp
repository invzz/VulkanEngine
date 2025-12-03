#include "3dEngine/SceneSerializer.hpp"

#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "3dEngine/components/LODComponent.hpp"

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

  SceneSerializer::SceneSerializer(GameObjectManager& manager, ResourceManager& resourceManager) : manager(manager), resourceManager(resourceManager) {}

  void SceneSerializer::serialize(const std::string& filepath)
  {
    nlohmann::json sceneJson;
    sceneJson["objects"] = nlohmann::json::array();

    for (const auto& [id, obj] : manager.getAllObjects())
    {
      nlohmann::json objJson;
      objJson["id"]   = obj.getId();
      objJson["name"] = obj.getName();

      // Transform
      objJson["transform"] = {{"translation", obj.transform.translation}, {"rotation", obj.transform.rotation}, {"scale", obj.transform.scale}};

      // PBR Material & Model
      if (obj.model)
      {
        objJson["modelPath"] = obj.model->getFilePath(); // Assuming Model has getFilePath()

        if (obj.pbrMaterial)
        {
          nlohmann::json matJson;
          matJson["albedo"]    = obj.pbrMaterial->albedo;
          matJson["metallic"]  = obj.pbrMaterial->metallic;
          matJson["roughness"] = obj.pbrMaterial->roughness;
          matJson["ao"]        = obj.pbrMaterial->ao;

          // Textures (if we can get their paths)
          // This might require extending PBRMaterial to store paths or getting them from the texture objects if they store it.
          // For now, let's assume we can't easily get texture paths back from the material unless we stored them.
          // Let's check PBRMaterial.hpp

          objJson["material"] = matJson;
        }
      }

      // Lights
      if (obj.pointLight)
      {
        objJson["pointLight"] = {{"intensity", obj.pointLight->intensity}, {"color", obj.color}, {"radius", obj.transform.scale.x}};
      }

      if (obj.directionalLight)
      {
        objJson["directionalLight"] = {{"intensity", obj.directionalLight->intensity}, {"color", obj.color}};
      }

      if (obj.spotLight)
      {
        objJson["spotLight"] = {{"intensity", obj.spotLight->intensity},
                                {"color", obj.color},
                                {"innerAngle", obj.spotLight->innerCutoffAngle},
                                {"outerAngle", obj.spotLight->outerCutoffAngle}};
      }

      // LOD Component
      if (obj.lodComponent)
      {
        nlohmann::json lodJson = nlohmann::json::array();
        for (const auto& level : obj.lodComponent->levels)
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

    // Clear existing objects? Or just add?
    // Usually loading a scene clears the current one.
    manager.clear();

    if (sceneJson.contains("objects"))
    {
      {
        for (const auto& objJson : sceneJson["objects"])
        {
          std::string name = objJson.value("name", "GameObject");

          // Create object
          // We can't easily use the static make* methods because the object might have mixed components (though unlikely in this engine's current design)
          // But the make* methods are just helpers. We can create a raw GameObject and add components.

          // However, GameObject constructor is private and create() is static.
          GameObject obj = GameObject::create(name);

          // Transform
          if (objJson.contains("transform"))
          {
            auto& t                   = objJson["transform"];
            obj.transform.translation = t.value("translation", glm::vec3(0.0f));
            obj.transform.rotation    = t.value("rotation", glm::vec3(0.0f));
            obj.transform.scale       = t.value("scale", glm::vec3(1.0f));
          }

          // Model & Material
          if (objJson.contains("modelPath"))
          {
            std::string modelPath = objJson["modelPath"];
            // Load model using ResourceManager
            // We need to know if it has textures/materials/morphs.
            // For now, assume defaults or store them in JSON.
            // Let's assume standard PBR object loading for now.
            obj.model = resourceManager.loadModel(modelPath, true, true, true); // Enable everything by default

            if (objJson.contains("material"))
            {
              auto& matJson              = objJson["material"];
              obj.pbrMaterial            = std::make_unique<PBRMaterial>();
              obj.pbrMaterial->albedo    = matJson.value("albedo", glm::vec3(1.0f));
              obj.pbrMaterial->metallic  = matJson.value("metallic", 0.0f);
              obj.pbrMaterial->roughness = matJson.value("roughness", 0.5f);
              obj.pbrMaterial->ao        = matJson.value("ao", 1.0f);

              // Re-binding textures would be needed here if we saved them.
              // Since we didn't save texture paths (yet), we rely on the model loader or default material.
              // If the model loader loaded materials, they are in the model, but PBRMaterial component overrides?
              // Actually, GameObject has a pbrMaterial component.
            }
          }

          // Lights
          if (objJson.contains("pointLight"))
          {
            auto& pl                  = objJson["pointLight"];
            obj.pointLight            = std::make_unique<PointLightComponent>();
            obj.pointLight->intensity = pl.value("intensity", 1.0f);
            obj.color                 = pl.value("color", glm::vec3(1.0f));
            obj.transform.scale.x     = pl.value("radius", 0.1f);
          }

          if (objJson.contains("directionalLight"))
          {
            auto& dl                        = objJson["directionalLight"];
            obj.directionalLight            = std::make_unique<DirectionalLightComponent>();
            obj.directionalLight->intensity = dl.value("intensity", 1.0f);
            obj.color                       = dl.value("color", glm::vec3(1.0f));
          }

          if (objJson.contains("spotLight"))
          {
            auto& sl                        = objJson["spotLight"];
            obj.spotLight                   = std::make_unique<SpotLightComponent>();
            obj.spotLight->intensity        = sl.value("intensity", 1.0f);
            obj.color                       = sl.value("color", glm::vec3(1.0f));
            obj.spotLight->innerCutoffAngle = sl.value("innerAngle", 12.5f);
            obj.spotLight->outerCutoffAngle = sl.value("outerAngle", 17.5f);
          }

          // LOD Component
          if (objJson.contains("lodComponent"))
          {
            obj.lodComponent = std::make_unique<LODComponent>();
            for (const auto& levelJson : objJson["lodComponent"])
            {
              float       distance  = levelJson.value("distance", 0.0f);
              std::string modelPath = levelJson.value("modelPath", "");
              if (!modelPath.empty())
              {
                auto model = resourceManager.loadModel(modelPath, true, true, true);
                obj.lodComponent->levels.push_back({model, distance});
              }
            }
          }

          manager.addObject(std::move(obj));
        }
      }

      return true;
    }
    return false;
  }

} // namespace engine
