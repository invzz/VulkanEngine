#pragma once

#include "../Component.hpp"

namespace engine {

  struct PointLightComponent : public Component
  {
    float intensity{1.0f};
  };

} // namespace engine
