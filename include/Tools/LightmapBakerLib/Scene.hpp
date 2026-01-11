#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace LightmapBaker {

  struct Light
  {
    glm::vec3 color{1.0f};
    float     intensity{1.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    bool      bake{false};
  };

  struct Object
  {
    std::string                id;
    std::string                name;
    std::optional<std::string> modelPath;
    glm::mat4                  transform{1.0f};
  };

  struct Scene
  {
    std::vector<Light>  lights;
    std::vector<Object> objects;

    // Load scene JSON and normalize into the Scene struct. Returns true on success.
    bool loadFromFile(const std::string& path, std::string* outErr = nullptr);
  };

} // namespace LightmapBaker
