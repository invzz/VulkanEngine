#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

int main(int argc, char** argv)
{
  namespace fs = std::filesystem;
  fs::create_directories("assets/scenes");

  nlohmann::json scene;
  scene["version"]          = 1;
  scene["assets"]           = nlohmann::json::object();
  scene["assets"]["models"] = nlohmann::json::array({"models/example_model.gltf"});

  scene["objects"] = nlohmann::json::array();
  nlohmann::json obj;
  obj["id"]        = "object_01";
  obj["mesh"]      = "models/example_model.gltf";
  obj["material"]  = "mat_01";
  obj["transform"] = {{"translation", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0, 1.0}}, {"scale", {1.0, 1.0, 1.0}}};
  obj["lighting"]  = {{"mobility", "Static"}};
  scene["objects"].push_back(obj);

  // Write scene.json
  std::ofstream out("assets/scenes/demo_scene.json");
  if (!out)
  {
    std::cerr << "Failed to open output file\n";
    return 1;
  }
  out << scene.dump(2) << std::endl;
  out.close();
  std::cout << "Wrote assets/scenes/demo_scene.json\n";

  // Write generated scene_lightmaps.json as a minimal example
  nlohmann::json lm;
  lm["version"]                       = 1;
  lm["lightmapBindings"]              = nlohmann::json::object();
  lm["lightmapBindings"]["object_01"] = {{"lightmapId", "lm_000"}, {"uvChannel", 1}, {"uvScale", {0.25, 0.25}}, {"uvOffset", {0.5, 0.0}}};
  lm["lightmaps"]                     = nlohmann::json::array();
  lm["lightmaps"].push_back({{"id", "lm_000"}, {"file", "lightmaps/lm_000_atlas.vtex"}, {"format", "vtex"}, {"resolution", {2048, 2048}}, {"paddingPx", 8}, {"usage", "Lightmap"}});

  std::ofstream out2("assets/scenes/demo_scene_lightmaps.json");
  if (!out2)
  {
    std::cerr << "Failed to open lightmap manifest file\n";
    return 1;
  }
  out2 << lm.dump(2) << std::endl;
  out2.close();
  std::cout << "Wrote assets/scenes/demo_scene_lightmaps.json\n";

  return 0;
}
