#pragma once

#include <cstdint>

#include "glm/vec3.hpp"

namespace engine {

  struct BakeTexel
  {
    glm::vec3 radiance{0.0f, 0.0f, 0.0f};
    uint8_t   valid{0}; // 0 = invalid, 1 = valid
  };

} // namespace engine
