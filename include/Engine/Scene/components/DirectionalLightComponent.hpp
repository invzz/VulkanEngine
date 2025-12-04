#pragma once

#include <glm/glm.hpp>

#include "../Component.hpp"

namespace engine {

  struct DirectionalLightComponent : public Component
  {
    float     intensity{1.0f};
    bool      useTargetPoint{false};
    glm::vec3 targetPoint{0.0f, 0.0f, 0.0f};
  };

} // namespace engine
