#pragma once

#include <glm/glm.hpp>

namespace engine {

  struct DirectionalLightComponent
  {
    float     intensity{1.0f};
    bool      useTargetPoint{false};
    glm::vec3 targetPoint{0.0f, 0.0f, 0.0f};
  };

} // namespace engine
