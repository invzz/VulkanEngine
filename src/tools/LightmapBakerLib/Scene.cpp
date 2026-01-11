#include "Tools/LightmapBakerLib/Scene.hpp"

#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace LightmapBaker {

  static glm::mat4 buildTransformFromJson(const nlohmann::json& t)
  {
    glm::mat4 mat(1.0f);
    if (t.contains("scale") && t["scale"].is_array() && t["scale"].size() >= 3)
    {
      float sx = t["scale"][0].is_number() ? t["scale"][0].get<float>() : 1.0f;
      float sy = t["scale"][1].is_number() ? t["scale"][1].get<float>() : 1.0f;
      float sz = t["scale"][2].is_number() ? t["scale"][2].get<float>() : 1.0f;
      mat      = glm::scale(mat, glm::vec3(sx, sy, sz));
    }
    if (t.contains("rotation") && t["rotation"].is_array())
    {
      const auto& r = t["rotation"];
      if (r.size() >= 4 && r[0].is_number() && r[1].is_number() && r[2].is_number() && r[3].is_number())
      {
        glm::quat q(r[3].get<float>(), r[0].get<float>(), r[1].get<float>(), r[2].get<float>());
        mat = mat * glm::mat4_cast(q);
      }
      else if (r.size() >= 3 && r[0].is_number() && r[1].is_number() && r[2].is_number())
      {
        glm::vec3 e(r[0].get<float>(), r[1].get<float>(), r[2].get<float>());
        glm::quat q = glm::quat(e);
        mat         = mat * glm::mat4_cast(q);
      }
    }
    if (t.contains("translation") && t["translation"].is_array() && t["translation"].size() >= 3)
    {
      float tx = t["translation"][0].is_number() ? t["translation"][0].get<float>() : 0.0f;
      float ty = t["translation"][1].is_number() ? t["translation"][1].get<float>() : 0.0f;
      float tz = t["translation"][2].is_number() ? t["translation"][2].get<float>() : 0.0f;
      mat      = glm::translate(mat, glm::vec3(tx, ty, tz));
    }
    return mat;
  }

  bool Scene::loadFromFile(const std::string& path, std::string* outErr)
  {
    try
    {
      std::ifstream in(path);
      if (!in)
      {
        if (outErr) *outErr = "Failed to open scene file";
        return false;
      }
      nlohmann::json sj;
      in >> sj;

      objects.clear();
      lights.clear();

      if (sj.contains("objects") && sj["objects"].is_array())
      {
        for (const auto& obj : sj["objects"])
        {
          // Lights
          if (obj.contains("directionalLight") && obj["directionalLight"].is_object())
          {
            const auto& dl = obj["directionalLight"];
            Light       L;
            L.bake = dl.value("bake", false);
            if (dl.contains("color") && dl["color"].is_array() && dl["color"].size() >= 3)
            {
              L.color.r = dl["color"][0].get<float>();
              L.color.g = dl["color"][1].get<float>();
              L.color.b = dl["color"][2].get<float>();
            }
            L.intensity = dl.value("intensity", 1.0f);
            // derive direction from object transform if present
            if (obj.contains("transform") && obj["transform"].is_object())
            {
              glm::mat4 t = buildTransformFromJson(obj["transform"]);
              // use -Y in local space transformed by rotation
              glm::vec3 dir = glm::normalize(glm::vec3(t * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));
              L.direction   = dir;
            }
            lights.push_back(L);
          }

          // Objects
          Object O;
          if (obj.contains("id") && obj["id"].is_string())
            O.id = obj["id"].get<std::string>();
          else if (obj.contains("id") && obj["id"].is_number())
            O.id = std::to_string(obj["id"].get<int>());
          else
            O.id = "";

          if (obj.contains("name") && obj["name"].is_string()) O.name = obj["name"].get<std::string>();

          if (obj.contains("modelPath") && obj["modelPath"].is_string())
            O.modelPath = obj["modelPath"].get<std::string>();
          else if (obj.contains("mesh") && obj["mesh"].is_string())
            O.modelPath = obj["mesh"].get<std::string>();

          if (obj.contains("transform") && obj["transform"].is_object()) O.transform = buildTransformFromJson(obj["transform"]);

          // Only include objects that have modelPath for baking; leave cameras/lights in objects list but modelPath empty
          objects.push_back(O);
        }
      }

      return true;
    }
    catch (const std::exception& e)
    {
      if (outErr) *outErr = e.what();
      return false;
    }
  }

} // namespace LightmapBaker
