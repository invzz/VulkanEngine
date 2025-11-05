#pragma once

#include "SimulationConfig.hpp"

namespace engine {

  struct Transform2DComponent;

  float massFromScale(const Transform2DComponent& transform);
  float uniformScaleForMass(float mass);
  float minimumSplitMass();

} // namespace engine
