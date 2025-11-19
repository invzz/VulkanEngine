#pragma once

namespace engine {

  struct SpotLightComponent
  {
    float intensity{1.0f};
    float innerCutoffAngle{12.5f}; // Inner cone angle in degrees
    float outerCutoffAngle{17.5f}; // Outer cone angle in degrees
    float constantAttenuation{1.0f};
    float linearAttenuation{0.09f};
    float quadraticAttenuation{0.032f};
  };

} // namespace engine
