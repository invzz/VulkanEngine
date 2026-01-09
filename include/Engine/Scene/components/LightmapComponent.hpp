#pragma once

#include <glm/glm.hpp>
#include <string>

namespace engine {

  struct LightmapComponent
  {
    std::string lightmapId;
    int         uvChannel = 1;
    glm::vec2   uvScale{1.0f, 1.0f};
    glm::vec2   uvOffset{0.0f, 0.0f};
    int         textureIndex = -1; // Optional runtime texture index if loaded
  };

} // namespace engine
