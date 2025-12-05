#pragma once

#include <glm/glm.hpp>

#include "../Component.hpp"

namespace engine {

  struct DirectionalLightComponent
  {
    float     intensity{1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    bool      useTargetPoint{false};
    glm::vec3 targetPoint{0.0f, 0.0f, 0.0f};
  };

} // namespace engine
