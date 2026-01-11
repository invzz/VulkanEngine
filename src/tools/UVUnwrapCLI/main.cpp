#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Tools/UVUnwrap/UVUnwrap.hpp"

using json   = nlohmann::json;
namespace fs = std::filesystem;

static glm::mat4 buildTransformFromJson(const json& t)
{
  glm::mat4 mat(1.0f);

  // scale: expect [sx, sy, sz]
  if (t.contains("scale") && t["scale"].is_array() && t["scale"].size() >= 3)
  {
    const auto s  = t["scale"];
    float      sx = s[0].is_number() ? s[0].get<float>() : 1.0f;
    float      sy = s[1].is_number() ? s[1].get<float>() : 1.0f;
    float      sz = s[2].is_number() ? s[2].get<float>() : 1.0f;
    mat           = glm::scale(mat, glm::vec3(sx, sy, sz));
  }

  // rotation: support either quaternion [x,y,z,w] or Euler angles [x,y,z] (radians)
  if (t.contains("rotation") && t["rotation"].is_array())
  {
    const auto r = t["rotation"];
    if (r.size() >= 4 && r[0].is_number() && r[1].is_number() && r[2].is_number() && r[3].is_number())
    {
      // Quaternion stored as [x,y,z,w]
      glm::quat q(r[3].get<float>(), r[0].get<float>(), r[1].get<float>(), r[2].get<float>());
      mat = mat * glm::mat4_cast(q);
    }
    else if (r.size() >= 3 && r[0].is_number() && r[1].is_number() && r[2].is_number())
    {
      // Euler angles [x,y,z] (rotation order: X(pitch), Y(yaw), Z(roll) as used elsewhere)
      glm::vec3 e(r[0].get<float>(), r[1].get<float>(), r[2].get<float>());
      glm::quat q = glm::quat(e);
      mat         = mat * glm::mat4_cast(q);
    }
    else
    {
      // Unexpected rotation format: ignore
    }
  }

  // translation: expect [tx, ty, tz]
  if (t.contains("translation") && t["translation"].is_array() && t["translation"].size() >= 3)
  {
    const auto tr = t["translation"];
    float      tx = tr[0].is_number() ? tr[0].get<float>() : 0.0f;
    float      ty = tr[1].is_number() ? tr[1].get<float>() : 0.0f;
    float      tz = tr[2].is_number() ? tr[2].get<float>() : 0.0f;
    mat           = glm::translate(mat, glm::vec3(tx, ty, tz));
  }

  return mat;
}

int main(int argc, char** argv)
{
  std::string inputPath  = "assets/scenes/test/uv_unwrap_input.json";
  std::string outputPath = "assets/scenes/test/generated_scene_lightmaps.json";

  if (argc >= 2) inputPath = argv[1];
  if (argc >= 3) outputPath = argv[2];

  fs::create_directories(fs::path(outputPath).parent_path());

  std::ifstream in(inputPath);
  if (!in)
  {
    std::cerr << "Failed to open input: " << inputPath << '\n';
    return 1;
  }

  json j;
  in >> j;

  // Parse meshes
  std::vector<tools::uvunwrap::MeshDecl>  meshes;
  std::unordered_map<std::string, size_t> meshIndexById;
  if (j.contains("meshes"))
  {
    for (const auto& m : j["meshes"])
    {
      tools::uvunwrap::MeshDecl md;
      // positions array
      auto               pos = m["positions"];
      std::vector<float> posbuf;
      posbuf.reserve(pos.size());
      for (const auto& v : pos)
      {
        if (!v.is_number())
        {
          std::cerr << "UVUnwrapCLI: warning: non-numeric position encountered, substituting 0\n";
          posbuf.push_back(0.0f);
        }
        else
        {
          posbuf.push_back(v.get<float>());
        }
      }
      // indices
      auto                  idx = m["indices"];
      std::vector<uint32_t> idxbuf;
      for (const auto& v : idx)
      {
        if (!v.is_number())
        {
          std::cerr << "UVUnwrapCLI: warning: non-numeric index encountered, substituting 0\n";
          idxbuf.push_back(0u);
        }
        else
        {
          idxbuf.push_back(v.get<uint32_t>());
        }
      }

      // store buffers in vectors owned by md via new allocations to remain alive
      float* pptr = new float[posbuf.size()];
      memcpy(pptr, posbuf.data(), posbuf.size() * sizeof(float));
      uint32_t* iptr = new uint32_t[idxbuf.size()];
      memcpy(iptr, idxbuf.data(), idxbuf.size() * sizeof(uint32_t));

      md.vertexPositionData = pptr;
      md.vertexCount        = static_cast<uint32_t>(posbuf.size() / 3);
      md.vertexStride       = sizeof(float) * 3;

      md.indexData   = iptr;
      md.indexCount  = static_cast<uint32_t>(idxbuf.size());
      md.indexStride = sizeof(uint32_t);

      meshIndexById[m["id"].get<std::string>()] = meshes.size();
      meshes.push_back(md);
    }
  }

  // Parse instances
  std::vector<std::pair<tools::uvunwrap::MeshDecl, glm::mat4>> instances;
  if (j.contains("instances"))
  {
    for (const auto& inst : j["instances"])
    {
      std::string id     = inst["id"].get<std::string>();
      std::string meshId = inst["mesh"].get<std::string>();
      if (meshIndexById.find(meshId) == meshIndexById.end())
      {
        std::cerr << "Unknown mesh id: " << meshId << '\n';
        continue;
      }
      size_t    mid = meshIndexById[meshId];
      glm::mat4 xf  = buildTransformFromJson(inst["transform"]);
      instances.push_back({meshes[mid], xf});
    }
  }

  int      paddingPx  = 4;
  uint32_t resolution = 0;
  if (j.contains("paddingPx")) paddingPx = j["paddingPx"].get<int>();
  if (j.contains("resolution")) resolution = j["resolution"].get<uint32_t>();

  auto mappings = tools::uvunwrap::generateInstanceMappings(instances, paddingPx, resolution);

  // Compose scene_lightmaps.json
  json lm;
  lm["version"]          = 1;
  lm["lightmapBindings"] = json::object();
  lm["lightmaps"]        = json::array();

  // Single lightmap id for this run
  std::string lmId         = "lm_000";
  json        lightmapMeta = json::object();
  lightmapMeta["id"]       = lmId;
  lightmapMeta["file"]     = "lightmaps/" + lmId + "_atlas.vtex";
  lightmapMeta["format"]   = "vtex";
  if (!mappings.empty())
  {
    lightmapMeta["resolution"] = {(int)mappings[0].atlasWidth, (int)mappings[0].atlasHeight};
  }
  else
  {
    lightmapMeta["resolution"] = {0, 0};
  }
  lightmapMeta["paddingPx"] = paddingPx;
  lightmapMeta["usage"]     = "Lightmap";
  lm["lightmaps"].push_back(lightmapMeta);

  // Emit bindings
  for (size_t i = 0; i < instances.size() && i < mappings.size(); ++i)
  {
    // instance id comes from input order
    std::string iid = j["instances"][i]["id"].get<std::string>();
    json        bind;
    bind["lightmapId"]          = lmId;
    bind["uvChannel"]           = 1;
    bind["uvScale"]             = {mappings[i].uvScale.x, mappings[i].uvScale.y};
    bind["uvOffset"]            = {mappings[i].uvOffset.x, mappings[i].uvOffset.y};
    lm["lightmapBindings"][iid] = bind;
  }

  std::ofstream out(outputPath);
  if (!out)
  {
    std::cerr << "Failed to open output file: " << outputPath << '\n';
    return 1;
  }
  out << lm.dump(2) << std::endl;
  out.close();
  std::cout << "Wrote " << outputPath << '\n';

  return 0;
}
