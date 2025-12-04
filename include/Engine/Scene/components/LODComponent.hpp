#pragma once

#include <memory>
#include <vector>

#include "../Component.hpp"
#include "Engine/Resources/Model.hpp"

namespace engine {

  struct LODLevel
  {
    std::shared_ptr<Model> model;
    float                  distance; // Distance at which this LOD becomes active
  };

  struct LODComponent : public Component
  {
    std::vector<LODLevel> levels; // Should be sorted by distance
  };

} // namespace engine
