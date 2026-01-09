#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
  class Scene;
}

namespace engine::scene {

  struct LightmapInfo
  {
    std::string      id;
    std::string      file;
    std::string      format;
    std::vector<int> resolution{0, 0};
    int              paddingPx = 0;
    std::string      usage;
  };

  struct LightmapBinding
  {
    std::string lightmapId;
    int         uvChannel = 1;
    glm::vec2   uvScale{1.0f, 1.0f};
    glm::vec2   uvOffset{0.0f, 0.0f};
  };

  // Parse a generated scene-level lightmap manifest (scene_lightmaps.json)
  // Returns true on success and fills out the maps (id -> info) and (objectId -> binding)
  bool parseSceneLightmapManifest(const std::string& manifestPath, std::unordered_map<std::string, LightmapInfo>& outLightmaps, std::unordered_map<std::string, LightmapBinding>& outBindings);

  // Apply bindings to a scene: for each entity with a NameComponent equal to objectId, emplace LightmapComponent
  void applyBindingsToScene(const std::unordered_map<std::string, LightmapBinding>& bindings, engine::Scene& scene);

} // namespace engine::scene
