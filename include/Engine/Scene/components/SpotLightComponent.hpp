#pragma once

#include <glm/glm.hpp>

#include "../Component.hpp"

namespace engine {

  struct SpotLightComponent : public Component
  {
    float     intensity{1.0f};
    float     innerCutoffAngle{12.5f}; // Inner cone angle in degrees
    float     outerCutoffAngle{17.5f}; // Outer cone angle in degrees
    float     constantAttenuation{1.0f};
    float     linearAttenuation{0.09f};
    float     quadraticAttenuation{0.032f};
    bool      useTargetPoint{false};
    glm::vec3 targetPoint{0.0f, 0.0f, 0.0f};
  };

} // namespace engine
