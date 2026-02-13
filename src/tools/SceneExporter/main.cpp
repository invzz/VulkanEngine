#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  fs::create_directories("assets/scenes/test");

  nlohmann::json scene;
  scene["version"] = 1;
  scene["assets"] = nlohmann::json::object();
  scene["assets"]["models"] = nlohmann::json::array({"models/example_model.gltf"});

  scene["objects"] = nlohmann::json::array();
  nlohmann::json obj;
  obj["id"] = "object_01";
  obj["mesh"] = "models/example_model.gltf";
  obj["material"] = "mat_01";
  obj["transform"] = {{"translation", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0, 1.0}}, {"scale", {1.0, 1.0, 1.0}}};
  obj["lighting"] = {{"mobility", "Static"}};
  scene["objects"].push_back(obj);
  // Lights: include bake + lightType fields for exported authoring scenes
  scene["lights"] = nlohmann::json::array();
  nlohmann::json sun;
  sun["id"] = "sun_01";
  sun["type"] = "directional";
  sun["direction"] = {-0.5, -1.0, -0.3};
  sun["color"] = {1.0, 0.98, 0.92};
  sun["intensity"] = 3.14;
  sun["bake"] = true;
  sun["lightType"] = "static";
  scene["lights"].push_back(sun);

  nlohmann::json lamp;
  lamp["id"] = "lamp_01";
  lamp["type"] = "point";
  lamp["position"] = {2.0, 1.0, 0.5};
  lamp["color"] = {0.4, 0.6, 1.0};
  lamp["intensity"] = 0.8;
  lamp["bake"] = false;
  lamp["lightType"] = "dynamic";
  scene["lights"].push_back(lamp);
  // Write scene.json
  std::ofstream out("assets/scenes/demo_scene.json");
  if (!out) {
    std::cerr << "Failed to open output file\n";
    return 1;
  }
  out << scene.dump(2) << std::endl;
  out.close();
  std::cout << "Wrote assets/scenes/demo_scene.json\n";

  return 0;
}
