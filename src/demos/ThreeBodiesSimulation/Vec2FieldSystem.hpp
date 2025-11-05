#pragma once

#include <vector>

#include "GravityPhysicsSystem.hpp"

namespace engine {

  class Vec2FieldSystem
  {
  public:
    void update(const GravityPhysicsSystem& physicsSystem, std::vector<GameObject>& physicsObjs, std::vector<GameObject>& vectorField) const;
  };

} // namespace engine
